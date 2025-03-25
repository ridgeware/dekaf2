/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2017, Ridgeware, Inc.
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

/// @file kstringview.h
/// string view implementation

#include "kdefinitions.h"
#include "bits/kstring_view.h"
#include "bits/khash.h"
#include "ktemplate.h"
#include "kutf.h"
#include "kbit.h"
#include <fmt/format.h>
#include <cinttypes>
#include <functional>
#ifndef BOOST_NO_CXX98_FUNCTION_BASE
	#define BOOST_NO_CXX98_FUNCTION_BASE
#endif
#if DEKAF2_HAS_INCLUDE(<boost/container_hash/hash.hpp>)
	#include <boost/container_hash/hash.hpp>
#else
	#include <boost/functional/hash.hpp>
#endif
#include <string>
#include <vector>
#include <ostream>

// =================== configure with or without SIMD =====================

// MSC does not offer a test for __SSE4_2__ support in the compiler,
// so we simply always assume it is there for MSC

// GCC 6 and 7 have serious problems with inlining and intrinsics and
// ASAN attributes, and there is no solution except upgrading the compiler
//
// Therefore, in debug mode with gcc < 8 we simply switch SSE off and
// fall back to traditional code

#if (defined(__SSE4_2__) || defined _MSC_VER) \
 && (!DEKAF2_IS_GCC || (DEKAF2_GCC_VERSION_MAJOR >= 8 || defined(NDEBUG)))
	#define DEKAF2_FIND_FIRST_OF_USE_SIMD 1
#else
	#if DEKAF2_ARM || DEKAF2_ARM64
		// The x86 simd emulation through sse2neon would be 3-8 times slower,
		// therefore we do not use it. It can be enabled though by
		// setting DEKAF2_FIND_FIRST_OF_USE_SIMD to 1
//		#define DEKAF2_FIND_FIRST_OF_USE_SIMD 1
	#endif
#endif

// =================== configure string view base =====================

#if !defined(DEKAF2_HAS_STD_STRING_VIEW) \
	&& !defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	// we have to use our own string_view if we do not have C++17..
	#define DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW 1
#endif

// older gcc versions have the cpp17 flag, but their libstdc++ does not
// support the constexpr reverse iterators
#if defined(DEKAF2_HAS_FULL_CPP_17)
	#define DEKAF2_CONSTEXPR_REVERSE_ITERATORS constexpr
#else
	#define DEKAF2_CONSTEXPR_REVERSE_ITERATORS
#endif

DEKAF2_NAMESPACE_BEGIN

class KStringView;

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kFind(const KStringView haystack,
			 const KStringView needle,
			 size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kRFind(const KStringView haystack,
              const KStringView needle,
              size_t pos = std::string::npos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
size_t kFind(const KStringView haystack,
             char needle,
             size_t pos = 0) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
size_t kFindNot(const KStringView haystack,
                char needle,
                size_t pos = 0) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
inline
size_t kRFind(const KStringView haystack,
              char needle,
              size_t pos = std::string::npos) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
size_t kRFindNot(const KStringView haystack,
                 char needle,
                 size_t pos = std::string::npos) noexcept;
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kFindFirstOf(KStringView haystack,
                    const KStringView needles,
                    size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kFindFirstNotOf(KStringView haystack,
                       const KStringView needles,
                       size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kFindLastOf(KStringView haystack,
                   const KStringView needles,
                   size_t pos = npos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
size_t kFindLastNotOf(KStringView haystack,
                      const KStringView needles,
                      size_t pos = npos);
//-----------------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kStartsWith(const KStringView sInput, const KStringView sPattern) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kEndsWith(const KStringView sInput, const KStringView sPattern) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kContains(const KStringView sInput, const KStringView sPattern) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kContains(const KStringView sInput, char ch) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_NODISCARD DEKAF2_PUBLIC
bool kContainsWord(const KStringView sInput, const KStringView sPattern) noexcept;
//----------------------------------------------------------------------

// forward declarations
class KString;
class KStringViewZ;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf2's own string view class - a wrapper around std::string_view or
/// folly::StringPiece, or our own implementation. Handles most errors without
/// throwing and speeds up searching up to 50 times compared to std::string_view
/// implementations.
class DEKAF2_PUBLIC DEKAF2_GSL_POINTER(char) KStringView
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using rep_type               = DEKAF2_SV_NAMESPACE::string_view;
	using self_type              = KStringView;
	using self                   = KStringView;
	using size_type              = std::size_t;
	using iterator               = rep_type::iterator;
	using const_iterator         = iterator;
	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = reverse_iterator;
	using value_type             = rep_type::value_type;
	using difference_type        = rep_type::difference_type;
	using reference              = rep_type::reference;
	using const_reference        = reference;
	using pointer                = value_type*;
	using const_pointer          = const pointer;
	using traits_type            = rep_type::traits_type;

	static constexpr size_type npos = size_type(-1);

	//-----------------------------------------------------------------------------
	constexpr
	KStringView() = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const self_type& other) noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KStringView(const std::string& str) noexcept
	//-----------------------------------------------------------------------------
	    : m_rep(str.data(), str.size())
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(sv::string_view str) noexcept
	//-----------------------------------------------------------------------------
		: m_rep(str.data(), str.size())
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const value_type* s, size_type count) noexcept
	//-----------------------------------------------------------------------------
#if defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	: m_rep(s, count)
#else
	// std::string_view in clang and gnu libc++ is not resilient to nullptr assignment
	// - therefore we protect it
	: m_rep(s ? s : &s_0ch, s ? count : 0)
#endif
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const value_type* s) noexcept
	//-----------------------------------------------------------------------------
#if defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	: m_rep(s)
#else
	// std::string_view in clang and gnu libc++ is not resilient to nullptr assignment
	// - therefore we protect it
	: m_rep(s ? s : &s_0ch)
#endif
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const KStringViewZ& svz) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KStringView(const KString& str) noexcept;
	//-----------------------------------------------------------------------------

	// the iterator constructor of std::string_view is way too dangerous -
	// a simple std::string_view("one", "two") causes a buffer overflow -
	// we will not implement it for KStringView..
	//-----------------------------------------------------------------------------
	template <class It, class End,
		typename std::enable_if<
			std::is_convertible<End, std::size_t>::value == false &&
			std::is_same<typename std::iterator_traits<It>::value_type, value_type>::value == true
		, int>::type=0
	>
#if DEKAF2_HAS_CONSTRAINTS
	requires std::sized_sentinel_for<End, It>
		&& std::contiguous_iterator<It>
#endif
	DEKAF2_CONSTEXPR_20
	constexpr KStringView(It first, End last) noexcept
	//-----------------------------------------------------------------------------
	: m_rep(first, last - first)
	{
		static_assert(first == last, "the iterator constructor is not supported - please use the (const char*, std::size_t) constructor instead");
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(const self_type& other) noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(const KStringViewZ& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self& operator=(const KString& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(const value_type* other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	self& operator=(const std::string& other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(sv::string_view other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}

	//-----------------------------------------------------------------------------
	/// @return a std::string_view of this string view
	DEKAF2_NODISCARD_PEDANTIC constexpr
	sv::string_view ToStdView() const
	//-----------------------------------------------------------------------------
	{
		return m_rep;
	}

	//-----------------------------------------------------------------------------
	constexpr
	operator sv::string_view() const
	//-----------------------------------------------------------------------------
	{
		return ToStdView();
	}

	//-----------------------------------------------------------------------------
	operator std::string() const
	//-----------------------------------------------------------------------------
	{
		return std::string(data(), size());
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	operator fmt::string_view() const
	//-----------------------------------------------------------------------------
	{
		return fmt::string_view(data(), size());
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// empty content of this string view
	DEKAF2_CONSTEXPR_14
	void clear()
	//-----------------------------------------------------------------------------
	{
#if defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
		m_rep.clear();
#else
		m_rep.remove_prefix(size());
#endif
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// assign to string view from char pointer
	/// @param start the start of the string
	/// @param size the size of the string
	DEKAF2_CONSTEXPR_14
	void assign(const value_type* start, size_type size)
	//-----------------------------------------------------------------------------
	{
		m_rep = rep_type(start, size);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// assign to string view from char pointer
	/// @param start the start of the string
	/// @param end the end of the string (pointing on the character directly after the end)
	DEKAF2_CONSTEXPR_14
	void assign(const value_type* start, const value_type* end)
	//-----------------------------------------------------------------------------
	{
		assign(start, static_cast<size_type>(end - start));
	}

	//-----------------------------------------------------------------------------
	/// maximum size for this string view implementation
	DEKAF2_NODISCARD_PEDANTIC constexpr
	size_type max_size() const
	//-----------------------------------------------------------------------------
	{
		return npos - 1;
	}

	//-----------------------------------------------------------------------------
	/// returns size of this string view - synonym for length()
	DEKAF2_NODISCARD_PEDANTIC constexpr
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.size();
	}

	//-----------------------------------------------------------------------------
	/// returns length of this string view - synonym for size()
	DEKAF2_NODISCARD_PEDANTIC constexpr
	size_type length() const
	//-----------------------------------------------------------------------------
	{
		return size();
	}

	//-----------------------------------------------------------------------------
	/// returns true if size() == 0
	DEKAF2_NODISCARD constexpr
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.empty();
	}

	//-----------------------------------------------------------------------------
	/// test if the string is non-empty
	constexpr
	explicit operator bool() const
	//-----------------------------------------------------------------------------
	{
		return !empty();
	}

	//-----------------------------------------------------------------------------
	/// returns pointer on begin of the string view
	DEKAF2_NODISCARD_PEDANTIC constexpr
	const value_type* data() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.data();
	}

	//-----------------------------------------------------------------------------
	/// returns begin iterator
	DEKAF2_NODISCARD_PEDANTIC constexpr
	iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.begin();
	}

	//-----------------------------------------------------------------------------
	/// returns end iterator
	DEKAF2_NODISCARD_PEDANTIC constexpr
	iterator end() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.end();
	}

	//-----------------------------------------------------------------------------
	/// returns reverse begin iterator
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	reverse_iterator rbegin() const
	//-----------------------------------------------------------------------------
	{
		return reverse_iterator(end());
	}

	//-----------------------------------------------------------------------------
	/// returns reverse end iterator
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	reverse_iterator rend() const
	//-----------------------------------------------------------------------------
	{
		return reverse_iterator(begin());
	}

	//-----------------------------------------------------------------------------
	/// returns constant begin iterator
	DEKAF2_NODISCARD_PEDANTIC constexpr
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.cbegin();
	}

	//-----------------------------------------------------------------------------
	/// returns constant end iterator
	DEKAF2_NODISCARD_PEDANTIC constexpr
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.cend();
	}

	//-----------------------------------------------------------------------------
	/// returns constant reverse begin iterator
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	const_reverse_iterator crbegin() const
	//-----------------------------------------------------------------------------
	{
		return const_reverse_iterator(cend());
	}

	//-----------------------------------------------------------------------------
	/// returns constant reverse end iterator
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	const_reverse_iterator crend() const
	//-----------------------------------------------------------------------------
	{
		return const_reverse_iterator(cbegin());
	}

	//-----------------------------------------------------------------------------
	/// returns reference to first character. NUL if string view is empty, never throws.
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	const value_type& front() const noexcept
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(empty())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "front() is not available");
#endif
			return s_0ch;
		}
		return m_rep.front();
	}

	//-----------------------------------------------------------------------------
	/// returns reference to last character. NUL if string view is empty, never throws.
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	const value_type& back() const noexcept
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(empty())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "back() is not available");
#endif
			return s_0ch;
		}
		return m_rep.back();
	}

	//-----------------------------------------------------------------------------
	/// compares this string view with another
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_17
	int compare(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		// uses __builtin_memcmp(), OK
		return m_rep.compare(rep_type(other.data(), other.size()));
	}

	//-----------------------------------------------------------------------------
	/// compares part of this string view with another
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            self_type other) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(other);
	}

	//-----------------------------------------------------------------------------
	/// compares part of this string view with part of another
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            self_type other,
	            size_type pos2, size_type count2) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(other.substr(pos2, count2));
	}

	//-----------------------------------------------------------------------------
	/// compares this string view with a char string
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_17
	int compare(const value_type* str) const
	//-----------------------------------------------------------------------------
	{
		return compare(self_type(str));
	}

	//-----------------------------------------------------------------------------
	/// compares part of this string view with a char string
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            const value_type* str) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(self_type(str));
	}

	//-----------------------------------------------------------------------------
	/// compares part of this string view with part of a char string
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            const value_type* str, size_type count2) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(self_type(str, count2));
	}

	//-----------------------------------------------------------------------------
	/// copies this string view into a char string
	size_type copy(value_type* dest, size_type count, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	const value_type& operator[](size_t index) const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(index >= size())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "Index access out of range");
#endif
			return s_0ch;
		}
		return m_rep[index];
	}

	//-----------------------------------------------------------------------------
	/// checked index access, returns NUL on range error, never throws
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	const value_type& at(size_t index) const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(index >= size())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "Index access out of range");
#endif
			return s_0ch;
		}
		return m_rep[index];
	}

	//-----------------------------------------------------------------------------
	/// returns sub string of this string view, checks range, never throws
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	self_type substr(size_type pos = 0, size_type count = npos) const
	//-----------------------------------------------------------------------------
	{
		return DEKAF2_UNLIKELY(pos > size())
		  ? self_type()
		  : self_type(data() + pos, std::min(count, size() - pos));
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a sub-view of the current view (synonym for substr(), aliased for template use because
	/// KString has both and makes a difference
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	self_type ToView(size_type pos = 0, size_type count = npos) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos, count);
	}

	//-----------------------------------------------------------------------------
	/// shrinks the view by moving its start forward by n characters
	DEKAF2_CONSTEXPR_14
	void remove_prefix(size_type n)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(n > size()))
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "n > size()");
#endif
			n = size();
		}
		unchecked_remove_prefix(n);
	}

	//-----------------------------------------------------------------------------
	/// shrinks the view by moving its end backwards by n characters
	DEKAF2_CONSTEXPR_14
	void remove_suffix(size_type n)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(n > size()))
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "n > size()");
#endif
			n = size();
		}
		unchecked_remove_suffix(n);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// shrinks the view by moving its start forward if start is other string
	/// @return true if successful, else false
	DEKAF2_CONSTEXPR_14
	bool remove_prefix(const self_type other)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(starts_with(other)))
		{
			unchecked_remove_prefix(other.size());
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// shrinks the view by moving its end backwards if end is other string
	/// @return true if successful, else false
	DEKAF2_CONSTEXPR_14
	bool remove_suffix(const self_type other)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(ends_with(other)))
		{
			unchecked_remove_suffix(other.size());
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// shrinks the view by moving its start forward if start is other character
	/// @return true if successful, else false
	template<typename T, typename std::enable_if<std::is_same<T, std::remove_cv<value_type>::type>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_14
	bool remove_prefix(T ch)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(starts_with(ch)))
		{
			unchecked_remove_prefix(1);
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// shrinks the view by moving its end backwards if end is other character
	/// @return true if successful, else false
	template<typename T, typename std::enable_if<std::is_same<T, std::remove_cv<value_type>::type>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_14
	bool remove_suffix(T ch)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_LIKELY(ends_with(ch)))
		{
			unchecked_remove_suffix(1);
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// append other view to this. Views must overlap or be adjacent, but other has to be to the right of this
	/// @return true if successful, else false
	DEKAF2_CONSTEXPR_14
	bool append(const self_type other)
	//-----------------------------------------------------------------------------
	{
		if (other.empty())
		{
			return true;
		}

		if (empty())
		{
			assign(other.data(), other.size());
			return true;
		}

		if (data() > other.data())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "cannot append, other view starts before this");
#endif
			return false;
		}

		if (data() + size() < other.data())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "non-adjacent views cannot be merged");
#endif
			return false;
		}

		if (data() + size() >= other.data() + other.size())
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "right view is a subset of left");
#endif
			return false;
		}

		assign(data(), (other.data() - data()) + other.size());

		return true;
	}

	//--------------------------------------------------------------------------------
	// nonstandard
	/// merge another view with this, regardless from which side and how much overlap..
	/// @return true if successful, else false
	DEKAF2_CONSTEXPR_14
	bool Merge(self_type other)
	//--------------------------------------------------------------------------------
	{
		if (other.empty())
		{
			return true;
		}

		if (empty())
		{
			assign(other.data(), other.size());
			return true;
		}

		if (data() > other.data())
		{
			if (other.data() + other.size() < data())
			{
#ifndef NDEBUG
				Warn(DEKAF2_FUNCTION_NAME, "non-adjacent views cannot be merged");
#endif
				return false;
			}

			swap(other);
		}

		// now it is assured that this view starts before or at the same position as other

		if (data() + size() < other.data() )
		{
#ifndef NDEBUG
			Warn(DEKAF2_FUNCTION_NAME, "non-adjacent views cannot be merged");
#endif
			return false;
		}

		assign(data(), (other.data() - data()) + other.size());

		return true;
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string start with sPattern?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool starts_with(const self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kStartsWith(*this, other);
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string start with ch?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool starts_with(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return empty() ? false : m_rep.front() == ch;
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string end with sPattern?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool ends_with(const self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kEndsWith(*this, other);
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string end with ch?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool ends_with(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return empty() ? false : m_rep.back() == ch;
	}

	//-----------------------------------------------------------------------------
	// std::C++23
	/// does the string contain the sPattern?
	DEKAF2_NODISCARD
	bool contains(const self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, other);
	}

	//-----------------------------------------------------------------------------
	// std::C++23
	/// does the string contain the ch?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool contains(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, ch);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string start with sPattern? (Now deprecated, replace by starts_with())
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool StartsWith(const self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return starts_with(other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string end with sPattern? (Now deprecated, replace by ends_with())
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool EndsWith(const self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return ends_with(other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string contain the sPattern? (Now deprecated, replace by contains())
	DEKAF2_NODISCARD
	bool Contains(const self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string contain the ch? (Now deprecated, replace by contains())
	DEKAF2_NODISCARD
	bool Contains(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, ch);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in uppercase (UTF8)
	DEKAF2_NODISCARD
	KString ToUpper() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in lowercase (UTF8)
	DEKAF2_NODISCARD
	KString ToLower() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
	DEKAF2_NODISCARD
	KString ToUpperLocale() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	DEKAF2_NODISCARD
	KString ToLowerLocale() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in uppercase assuming ASCII encoding
	DEKAF2_NODISCARD
	KString ToUpperASCII() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in lowercase assuming ASCII encoding
	DEKAF2_NODISCARD
	KString ToLowerASCII() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// match with regular expression and return the overall match (group 0)
	DEKAF2_NODISCARD
	KStringView MatchRegex(const KStringView sRegEx, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// match with regular expression and return all match groups
	DEKAF2_NODISCARD
	std::vector<KStringView> MatchRegexGroups(const KStringView sRegEx, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns leftmost iCount chars of string
	DEKAF2_NODISCARD
	KStringView Left(size_type iCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at iStart for iCount chars
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	KStringView Mid(size_type iStart, size_type iCount = npos) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns rightmost iCount chars of string
	DEKAF2_NODISCARD
	KStringView Right(size_type iCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns leftmost iCount codepoints of string
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	KStringView LeftUTF8(size_type iCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at codepoint iStart for iCount codepoints
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	KStringView MidUTF8(size_type iStart, size_type iCount = npos) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns rightmost iCount UTF8 codepoints of string
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	KStringView RightUTF8(size_type iCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns KCcodePoint at UTF8 position iCount
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	KCodePoint AtUTF8(size_type iCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns true if string contains UTF8 runs
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	bool HasUTF8() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns the count of unicode codepoints (or, UTF8 sequences)
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	size_type SizeUTF8() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes white space from the left of the string
	self& TrimLeft();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes chTrim from the left of the string
	self& TrimLeft(value_type chTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes any character in sTrim from the left of the string
	self& TrimLeft(const KStringView sTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes white space from the right of the string
	self& TrimRight();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes chTrim from the right of the string
	self& TrimRight(value_type chTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes any character in sTrim from the right of the string
	self& TrimRight(const KStringView sTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes white space from the left and right of the string
	self& Trim();
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes chTrim from the left and right of the string
	self& Trim(value_type chTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// removes any character in sTrim from the left and right of the string
	self& Trim(const KStringView sTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Clip removing sClipAt and everything to its right if found; otherwise do not alter the string
	bool ClipAt(const KStringView sClipAt);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(const KStringView sClipAtReverse);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Splits string into token container using delimiters, trim, and escape. Returned
	/// Container is a sequence, like a vector, or an associative container like a map.
	/// @return a new Container. Default is a std::vector<KStringView>.
	/// @param svDelim a string view of delimiter characters. Defaults to ",".
	/// @param svPairDelim exists only for associative containers: a string view that is used to separate keys and values in the sequence. Defaults to "=".
	/// @param svTrim a string containing chars to remove from both token ends. Defaults to " \f\n\r\t\v\b".
	/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
	/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
	/// taken for found spaces if defined as delimiter). Defaults to false.
	/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
	/// double quotes are treated as literal chars, and quotes themselves are removed.
	/// No trimming is applied inside the quotes (but outside). The quote has to be the
	/// first character after applied trimming, and trailing content after the closing quote
	/// is not considered part of the token. Defaults to false.
	template<typename T = std::vector<KStringView>, typename... Parms>
	DEKAF2_NODISCARD
	T Split(Parms&&... parms) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// swaps the contents with another string view
	void swap(self_type& other)
	//-----------------------------------------------------------------------------
	{
		using std::swap;
		swap(m_rep, other.m_rep);
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: output the hash value of instance by calling std::hash() for the type
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	std::size_t Hash() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: output the hash value of a lowercase ASCII version of the instance by calling std::hash() for the type
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14
	std::size_t CaseHash() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin or end
	self& erase(size_type pos = 0, size_type n = npos);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if position is at begin or end
	iterator erase(const_iterator position)
	//-----------------------------------------------------------------------------
	{
		bool bToStart = position == begin();
		erase(static_cast<size_type>(position - begin()), 1);
		return bToStart ? begin() : end();
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin or end
	iterator erase(const_iterator first, const_iterator last)
	//-----------------------------------------------------------------------------
	{
		bool bToStart = first == begin();
		erase(static_cast<size_type>(first - begin()), static_cast<size_type>(last - first));
		return bToStart ? begin() : end();
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: caseless ASCII search
	DEKAF2_NODISCARD
	size_type FindCaselessASCII(const self_type str, size_type pos = 0) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: returns true if caseless ASCII search of another string is successful
	DEKAF2_NODISCARD
	bool ContainsCaselessASCII(const self_type str) const noexcept
	//-----------------------------------------------------------------------------
	{
		return FindCaselessASCII(str) != npos;
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find(const self_type str, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFind(*this, str, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	size_type find(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFind(*this, ch, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type rfind(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kRFind(*this, ch, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type rfind(const self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kRFind(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_first_of(const self_type sv, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindFirstOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_first_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_first_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	size_type find_first_of(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find(ch, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_of(const self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindLastOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_of(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return rfind(ch, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_first_not_of(const self_type sv, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindFirstNotOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_first_not_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_first_not_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	size_type find_first_not_of(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindNot(*this, ch, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_not_of(const self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindLastNotOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_not_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC
	size_type find_last_not_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_14
	size_type find_last_not_of(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kRFindNot(*this, ch, pos);
	}

	//-----------------------------------------------------------------------------
	/// is string one of the values in sHaystack, delimited by iDelim?
	DEKAF2_NODISCARD
	bool In (const KStringView sHaystack, value_type iDelim=',') const;
	//-----------------------------------------------------------------------------

	// conversions

	//-----------------------------------------------------------------------------
	/// returns bool representation of the string:
	/// "true" --> true
	/// "false" --> false
	/// as well as non-0 --> true
	DEKAF2_NODISCARD
	bool Bool() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to int16_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int16_t Int16(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint16_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint16_t UInt16(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to int32_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int32_t Int32(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint32_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint32_t UInt32(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to int64_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int64_t Int64(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint64_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint64_t UInt64(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_INT128
	//-----------------------------------------------------------------------------
	/// convert to int128_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int128_t Int128(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint128_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint128_t UInt128(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// convert to float
	DEKAF2_NODISCARD
	float Float() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to double
	DEKAF2_NODISCARD
	double Double() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// returns true if sOther is same
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_14 
	bool Equal(KStringView sOther) const noexcept
	//-----------------------------------------------------------------------------
	{
		return size() == sOther.size() && !compare(sOther);
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
#ifdef DEKAF2_HAS_CPP_14
	DEKAF2_ALWAYS_INLINE
	DEKAF2_CONSTEXPR_14
#endif
	void unchecked_remove_prefix(size_type n)
	//-----------------------------------------------------------------------------
	{
		m_rep.remove_prefix(n);
	}

	//-----------------------------------------------------------------------------
#ifdef DEKAF2_HAS_CPP_14
	DEKAF2_ALWAYS_INLINE
	DEKAF2_CONSTEXPR_14
#endif
	void unchecked_remove_suffix(size_type n)
	//-----------------------------------------------------------------------------
	{
		m_rep.remove_suffix(n);
	}

	//-----------------------------------------------------------------------------
	void Warn(KStringView sWhere, KStringView sWhat) const;
	//-----------------------------------------------------------------------------

	rep_type m_rep; // NOLINT: clang-tidy wants us to make this private, but it does not know that we need it protected for KStringViewZ

	static constexpr value_type s_0ch = '\0'; // NOLINT: same as above

};

// ======================= comparisons ========================

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_same<T, KStringView>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator==(const T& left, const T& right)
//-----------------------------------------------------------------------------
{
	return left.Equal(right);
}

//-----------------------------------------------------------------------------
template<typename T,
		 typename std::enable_if<std::is_same<T, KStringView>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator!=(const T& left, const T& right)
//-----------------------------------------------------------------------------
{
	return !operator==(left, right);
}

//-----------------------------------------------------------------------------
template<typename T, typename U,
		 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true &&
                                 detail::is_kstringview_assignable<const U&, true>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator==(const T& left, const U& right)
//-----------------------------------------------------------------------------
{
	return KStringView(left).Equal(KStringView(right));
}

//-----------------------------------------------------------------------------
template<typename T, typename U,
		 typename std::enable_if<detail::is_kstringview_assignable<const T&, true>::value == true &&
                                 detail::is_kstringview_assignable<const U&, true>::value == true, int>::type = 0>
DEKAF2_CONSTEXPR_14
bool operator!=(const T& left, const U& right)
//-----------------------------------------------------------------------------
{
	return !operator==(left, right);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17 DEKAF2_PUBLIC
bool operator<(const KStringView left, const KStringView right)
//-----------------------------------------------------------------------------
{
	return left.compare(right) < 0;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17 DEKAF2_PUBLIC
bool operator>(const KStringView left, const KStringView right)
//-----------------------------------------------------------------------------
{
	return right < left;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17 DEKAF2_PUBLIC
bool operator<=(const KStringView left, const KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left > right);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17 DEKAF2_PUBLIC
bool operator>=(const KStringView left, const KStringView right)
//-----------------------------------------------------------------------------
{
	return !(right < left);
}

// ======================= end comparisons ========================


// ** KStringView is now completed **


using KStringViewPair = std::pair<KStringView, KStringView>;

namespace detail {

static constexpr KStringView kASCIISpaces { " \f\n\r\t\v\b" };

} // end of namespace detail

DEKAF2_NAMESPACE_END

#include "bits/kfindsetofchars.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
size_t kFind(
		const KStringView haystack,
        const char needle,
        size_t pos) noexcept
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	// we keep this inlined as then the compiler can evaluate const expressions
	// (memchr() is actually a compiler-builtin)
	const auto iHaystackSize = haystack.size();

	if (DEKAF2_UNLIKELY(pos >= iHaystackSize))
	{
		return KStringView::npos;
	}

#if defined(DEKAF2_IS_CLANG) || defined(DEKAF2_IS_GCC)
	auto ret = static_cast<const char*>(__builtin_memchr(haystack.data()+pos, needle, iHaystackSize-pos));
#else
	auto ret = static_cast<const char*>(memchr(haystack.data()+pos, needle, iHaystackSize-pos));
#endif

	if (DEKAF2_UNLIKELY(ret == nullptr))
	{
		return KStringView::npos;
	}

	return static_cast<size_t>(ret - haystack.data());
#else
	return static_cast<KStringView::rep_type>(haystack).find(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
size_t kFindNot(
		const KStringView haystack,
		const char needle,
		size_t pos) noexcept
//-----------------------------------------------------------------------------
{
	const auto iHaystackSize = haystack.size();

	if (DEKAF2_LIKELY(pos < iHaystackSize))
	{
		for (auto it = haystack.begin() + pos; it != haystack.end();)
		{
			if (*it++ != needle)
			{
				return it - haystack.begin() - 1;
			}
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
size_t kRFind(
        const KStringView haystack,
        const char needle,
        size_t pos) noexcept
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	const auto iHaystackSize = haystack.size();

	if (DEKAF2_UNLIKELY(!iHaystackSize))
	{
		return KStringView::npos;
	}

	if (pos >= iHaystackSize)
	{
		pos = iHaystackSize;
	}
	else
	{
		++pos;
	}

	auto found = static_cast<const char*>(memrchr(haystack.data(), needle, pos));

	if (DEKAF2_UNLIKELY(!found))
	{
		return KStringView::npos;
	}

	return static_cast<size_t>(found - haystack.data());
#else
	return static_cast<KStringView::rep_type>(haystack).rfind(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
size_t kRFindNot(
		const KStringView haystack,
		const char needle,
		size_t pos) noexcept
//-----------------------------------------------------------------------------
{
	const auto iHaystackSize = haystack.size();

	if (pos >= iHaystackSize)
	{
		pos = iHaystackSize;
	}
	else
	{
		++pos;
	}

	for (auto it = haystack.begin() + pos; it != haystack.begin();)
	{
		if (*--it != needle)
		{
			return it - haystack.begin();
		}
	}

	return KStringView::npos;
}

//-----------------------------------------------------------------------------
inline
size_t kFindFirstOf(
		KStringView haystack,
		const KStringView needles,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return KFindSetOfChars(needles).find_first_in(haystack, pos);
#else
	return static_cast<KStringView::rep_type>(haystack).find_first_of(needles, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindFirstNotOf(
		KStringView haystack,
		const KStringView needles,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return KFindSetOfChars(needles).find_first_not_in(haystack, pos);
#else
	return static_cast<KStringView::rep_type>(haystack).find_first_not_of(needles, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindLastOf(
		KStringView haystack,
		const KStringView needles,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return KFindSetOfChars(needles).find_last_in(haystack, pos);
#else
	return static_cast<KStringView::rep_type>(haystack).find_last_of(needles, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindLastNotOf(
		KStringView haystack,
		const KStringView needles,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return KFindSetOfChars(needles).find_last_not_in(haystack, pos);
#else
	return static_cast<KStringView::rep_type>(haystack).find_last_not_of(needles, pos);
#endif
}

//-----------------------------------------------------------------------------
/// Find delimiter chars prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter chars prefixed by odd number of escapes.
DEKAF2_PUBLIC
size_t kFindFirstOfUnescaped(const KStringView haystack,
							 const KFindSetOfChars& needles,
							 KStringView::value_type chEscape,
							 KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Find delimiter char prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter chars prefixed by odd number of escapes.
DEKAF2_PUBLIC
size_t kFindUnescaped(const KStringView haystack,
                      KStringView::value_type needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Find delimiter string prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter string prefixed by odd number of escapes.
DEKAF2_PUBLIC
size_t kFindUnescaped(const KStringView haystack,
					  const KStringView needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kStartsWith(const KStringView sInput, const KStringView sPattern) noexcept
//----------------------------------------------------------------------
{
	const auto iPatternSize = sPattern.size();

	if (DEKAF2_UNLIKELY(sInput.size() < iPatternSize))
	{
		return false;
	}

	if (DEKAF2_UNLIKELY(iPatternSize == 0))
	{
		return true;
	}

	return !std::memcmp(sInput.data(), sPattern.data(), iPatternSize);

} // kStartsWith

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kEndsWith(const KStringView sInput, const KStringView sPattern) noexcept
//----------------------------------------------------------------------
{
	const auto iInputSize   = sInput.size();
	const auto iPatternSize = sPattern.size();

	if (DEKAF2_UNLIKELY(iInputSize < iPatternSize))
	{
		return false;
	}

	if (DEKAF2_UNLIKELY(iPatternSize == 0))
	{
		return true;
	}

	return !std::memcmp(sInput.data() + iInputSize - iPatternSize, sPattern.data(), iPatternSize);

} // kEndsWith

//----------------------------------------------------------------------
inline DEKAF2_PUBLIC
bool kContains(const KStringView sInput, const KStringView sPattern) noexcept
//----------------------------------------------------------------------
{
	return kFind(sInput, sPattern) != KStringView::npos;
}

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC
bool kContains(const KStringView sInput, const char ch) noexcept
//----------------------------------------------------------------------
{
	return kFind(sInput, ch) != KStringView::npos;
}

inline namespace literals {

	/// provide a string literal for KStringView
	constexpr DEKAF2_PREFIX KStringView operator"" _ksv(const char *data, std::size_t size)
	{
		return {data, size};
	}

} // namespace literals

//-----------------------------------------------------------------------------
inline DEKAF2_PUBLIC
std::ostream& operator <<(std::ostream& stream, KStringView str)
//-----------------------------------------------------------------------------
{
	stream.write(str.data(), str.size());
	return stream;
}

DEKAF2_NAMESPACE_END

namespace std
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a std::hash for KStringView
	template<>
	struct hash<DEKAF2_PREFIX KStringView>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		using is_transparent = void;

		DEKAF2_CONSTEXPR_14 std::size_t operator()(DEKAF2_PREFIX KStringView s) const noexcept
		{
			return DEKAF2_PREFIX kHash(s.data(), s.size());
		}

		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return DEKAF2_PREFIX kHash(s);
		}
	};

} // namespace std

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
namespace boost {
#else
DEKAF2_NAMESPACE_BEGIN
#endif
	inline
	std::size_t hash_value(const DEKAF2_PREFIX KStringView& s)
	{
		return s.Hash();
	}
#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
}
#else
DEKAF2_NAMESPACE_END
#endif

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC std::size_t DEKAF2_PREFIX KStringView::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<DEKAF2_PREFIX KStringView>()(*this);
}

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14 DEKAF2_PUBLIC std::size_t DEKAF2_PREFIX KStringView::CaseHash() const
//----------------------------------------------------------------------
{
	return DEKAF2_PREFIX kCaseHash(data(), size());
}

#include "bits/kstringviewz.h"
#include "ksplit.h"
#include "kstring.h"
#include "kstringutils.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
constexpr
KStringView::KStringView(const KStringViewZ& svz) noexcept
//-----------------------------------------------------------------------------
: KStringView(svz.data(), svz.size())
{
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
KStringView& KStringView::operator=(const KStringViewZ& other)
//-----------------------------------------------------------------------------
{
	assign(other.data(), other.size());
	return *this;
}

//-----------------------------------------------------------------------------
inline
bool KStringView::In (const KStringView sHaystack, value_type iDelim) const
//-----------------------------------------------------------------------------
{
	return kStrIn (*this, sHaystack, iDelim);
}

//-----------------------------------------------------------------------------
inline
KStringView::KStringView(const KString& str) noexcept
//-----------------------------------------------------------------------------
: KStringView(str.data(), str.size())
{
}

//-----------------------------------------------------------------------------
inline
KStringView& KStringView::operator=(const KString& other)
//-----------------------------------------------------------------------------
{
	assign(other.data(), other.size());
	return *this;
}

//-----------------------------------------------------------------------------
inline
KString KStringView::ToUpper() const
//-----------------------------------------------------------------------------
{
	return kToUpper(*this);
}

//-----------------------------------------------------------------------------
inline
KString KStringView::ToLower() const
//-----------------------------------------------------------------------------
{
	return kToLower(*this);
}

//-----------------------------------------------------------------------------
inline
KString KStringView::ToUpperLocale() const
//-----------------------------------------------------------------------------
{
	return kToUpperLocale(*this);
}

//-----------------------------------------------------------------------------
inline
KString KStringView::ToLowerLocale() const
//-----------------------------------------------------------------------------
{
	return kToLowerLocale(*this);
}

//-----------------------------------------------------------------------------
inline
KString KStringView::ToUpperASCII() const
//-----------------------------------------------------------------------------
{
	return kToUpperASCII(*this);
}

//-----------------------------------------------------------------------------
inline
KString KStringView::ToLowerASCII() const
//-----------------------------------------------------------------------------
{
	return kToLowerASCII(*this);
}

//-----------------------------------------------------------------------------
inline
KStringView KStringView::Left(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kLeft<KStringView, KStringView>(*this, iCount);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
KStringView KStringView::Mid(size_type iStart, size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kMid<KStringView, KStringView>(*this, iStart, iCount);
}

//-----------------------------------------------------------------------------
inline
KStringView KStringView::Right(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kRight<KStringView, KStringView>(*this, iCount);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
KStringView KStringView::LeftUTF8(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kLeftUTF<KStringView, KStringView>(*this, iCount);
}

//-----------------------------------------------------------------------------
// nonstandard
/// returns substring starting at codepoint iStart for iCount codepoints
DEKAF2_CONSTEXPR_14
KStringView KStringView::MidUTF8(size_type iStart, size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kMidUTF<KStringView, KStringView>(*this, iStart, iCount);
}

//-----------------------------------------------------------------------------
// nonstandard
/// returns rightmost iCount UTF8 codepoints of string
DEKAF2_CONSTEXPR_14
KStringView KStringView::RightUTF8(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kRightUTF<KStringView, KStringView>(*this, iCount);
}

//-----------------------------------------------------------------------------
// nonstandard
/// returns KCcodePoint at UTF8 position iCount
DEKAF2_CONSTEXPR_14
KCodePoint KStringView::AtUTF8(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return kAtUTF(*this, iCount);
}

//-----------------------------------------------------------------------------
// nonstandard
/// returns true if string contains UTF8 runs
DEKAF2_CONSTEXPR_14
bool KStringView::HasUTF8() const
//-----------------------------------------------------------------------------
{
	return kHasUTF8(*this);
}

//-----------------------------------------------------------------------------
// nonstandard
/// returns the count of unicode codepoints (or, UTF8 sequences)
DEKAF2_CONSTEXPR_14
KStringView::size_type KStringView::SizeUTF8() const
//-----------------------------------------------------------------------------
{
	return kSizeUTF(*this);
}

//-----------------------------------------------------------------------------
template<typename T, typename...Parms>
T KStringView::Split(Parms&&... parms) const
//-----------------------------------------------------------------------------
{
	T Container;
	kSplit(Container, *this, std::forward<Parms>(parms)...);
	return Container;
}

DEKAF2_NAMESPACE_END

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE {

template <>
struct formatter<DEKAF2_PREFIX KStringView> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KStringView& String, FormatContext& ctx) const
	{
		return formatter<string_view>::format(string_view(String.data(), String.size()), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE
