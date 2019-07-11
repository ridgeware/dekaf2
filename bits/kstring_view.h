/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/// @file kstring_view.h
/// test for include file locations for std::string_view, and include if
/// available

#include "kcppcompat.h"

#if DEKAF2_HAS_CPP_17
	#if DEKAF2_HAS_INCLUDE(<string_view>)
		#include <string_view>
		#define DEKAF2_HAS_STD_STRING_VIEW 1
		#define DEKAF2_SV_NAMESPACE std
	#elif DEKAF2_HAS_INCLUDE(<experimental/string_view>)
		#include <experimental/string_view>
		#define DEKAF2_HAS_STD_STRING_VIEW 1
		#define DEKAF2_SV_NAMESPACE std::experimental
	#endif
#endif

#if !defined(DEKAF2_HAS_STD_STRING_VIEW) && !defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)

	/// tiny but nearly complete string_view implementation - it only does not have rfind() nor reverse iterators nor find_first/last_(not)_of()

	#define DEKAF2_SV_NAMESPACE dekaf2::detail::stringview
	#define DEKAF2_HAS_STD_STRING_VIEW 1
	#define DEKAF2_HAS_OWN_STRING_VIEW 1

	namespace dekaf2 {
	namespace detail {
	namespace stringview {

	class string_view {
	public:
		using CharT = char;
		using Traits = std::char_traits<CharT>;
		using traits_type = Traits;
		using value_type = CharT;
		using size_type = std::size_t;
		using const_pointer = const CharT*;
		using const_reference = const CharT&;
		using const_iterator = const_pointer;
		using iterator = const_iterator;
		using difference_type = typename std::iterator_traits<const char*>::difference_type;
		using reference = typename std::iterator_traits<const char*>::reference;
		static constexpr size_type npos = size_type(-1);

		constexpr
		string_view() noexcept
		: m_pszString(&m_chEmpty)
		, m_iSize(0)
		{}

		constexpr
		string_view(const string_view& other) noexcept
		: m_pszString(other.m_pszString)
		, m_iSize(other.m_iSize)
		{}

		string_view(const std::string& strStr)
		: m_pszString(strStr.c_str())
		, m_iSize(strStr.size())
		{}

		constexpr
		string_view(const char* pszStr)
		: m_pszString(pszStr ? pszStr : &m_chEmpty)
		, m_iSize(pszStr ? constexpr_strlen(pszStr) : 0)
		{}

		constexpr
		string_view(const char* pszStr, size_type iSize)
		: m_pszString(pszStr ? pszStr : &m_chEmpty)
		, m_iSize(pszStr ? iSize : 0)
		{}

		DEKAF2_CONSTEXPR_14
		string_view& operator=(const string_view& other) noexcept
		{
			m_pszString = other.m_pszString;
			m_iSize = other.m_iSize;
			return *this;
		}

		DEKAF2_CONSTEXPR_14
		void clear() noexcept
		{
			m_pszString = std::addressof(m_chEmpty);
			m_iSize = 0;
		}

		DEKAF2_CONSTEXPR_14
		size_type max_size() const noexcept
		{
			return size_type(-1) - 1;
		}

		DEKAF2_CONSTEXPR_14
		size_type size() const noexcept
		{
			return m_iSize;
		}

		DEKAF2_CONSTEXPR_14
		size_type length() const
		{
			return size();
		}

		DEKAF2_CONSTEXPR_14
		bool empty() const noexcept
		{
			return !size();
		}

		DEKAF2_CONSTEXPR_14
		iterator begin() const noexcept
		{
			return m_pszString;
		}

		DEKAF2_CONSTEXPR_14
		iterator end() const noexcept
		{
			return m_pszString + m_iSize;
		}

		DEKAF2_CONSTEXPR_14
		iterator cbegin() const noexcept
		{
			return begin();
		}

		DEKAF2_CONSTEXPR_14
		iterator cend() const noexcept
		{
			return end();
		}

		DEKAF2_CONSTEXPR_14
		const_reference front() const noexcept
		{
			return *begin();
		}

		DEKAF2_CONSTEXPR_14
		const_reference back() const noexcept
		{
			return *(end() - 1);
		}

		DEKAF2_CONSTEXPR_14
		const_pointer data() const noexcept
		{
			return begin();
		}

		DEKAF2_CONSTEXPR_14
		const_reference operator[](size_type pos) const
		{
			return *(begin() + pos);
		}

		DEKAF2_CONSTEXPR_14
		const_reference at(size_type pos) const
		{
			if (pos >= size())
			{
				throw std::out_of_range({});
			}
			return operator[](pos);
		}

		DEKAF2_CONSTEXPR_14
		void remove_prefix(size_type n)
		{
			if (n > m_iSize)
			{
				n = m_iSize;
			}
			m_pszString += n;
			m_iSize -= n;
		}

		DEKAF2_CONSTEXPR_14 void remove_suffix(size_type n)
		{
			if (n > m_iSize)
			{
				n = m_iSize;
			}
			m_iSize -= n;
		}

		void swap(string_view& other) noexcept
		{
			using std::swap; swap(*this, other);
		}

		DEKAF2_CONSTEXPR_14
		string_view substr(size_type pos = 0, size_type count = npos) const
		{
			return { m_pszString + pos, std::min(count, size() - pos) };
		}

		DEKAF2_CONSTEXPR_14
		int compare(string_view other) const noexcept
		{
			auto cmp = Traits::compare(data(), other.data(), std::min(size(), other.size()));
			return (cmp) ? cmp : size() - other.size();
		}

		DEKAF2_CONSTEXPR_14
		size_type find(string_view needle, size_type pos = 0) const noexcept
		{
			if (needle.size() == 1)
			{
				return find(needle.front(), pos);
			}
			if (pos >= size() || needle.empty() || needle.size() > (size() - pos))
			{
				return npos;
			}
			auto found = static_cast<const char*>(::memmem(data() + pos, size() - pos, needle.data(), needle.size()));
			return (found) ? static_cast<size_t>(found - data()) : npos;
		}

		DEKAF2_CONSTEXPR_14
		size_type find(CharT ch, size_type pos = 0) const noexcept
		{
			if (pos > size())
			{
				return npos;
			}
			auto found = static_cast<const char*>(::memchr(data() + pos, ch, size() - pos));
			return (found) ? static_cast<size_t>(found - data()) : npos;
		}

		DEKAF2_CONSTEXPR_14
		size_type find(const CharT* s, size_type pos, size_type count) const
		{
			return find(string_view(s, count), pos);
		}

		DEKAF2_CONSTEXPR_14
		size_type find(const CharT* s, size_type pos) const
		{
			return find(string_view(s), pos);
		}

	#if (DEKAF2_HAS_CPP_14)
		static
		constexpr
		size_type constexpr_strlen(const CharT* s)
		{
			const CharT* start = s;
			while (*s++);
			return s - start - 1;
		}
	#else
		static
		constexpr
		size_type constexpr_strlen_1(const CharT* s, size_type len)
		{
			return *s == 0 ? len : constexpr_strlen_1(s + 1, len + 1);
		}

		static
		constexpr
		size_t constexpr_strlen(const CharT* s)
		{
			return constexpr_strlen_1(s, 0);
		}
	#endif

	private:

		static constexpr char m_chEmpty = 0;
		const char* m_pszString;
		std::size_t m_iSize;

	}; // string_view

	DEKAF2_CONSTEXPR_14
	inline bool operator==(string_view left, string_view right) noexcept
	{
		return left.size() == right.size()
			&& (left.data() == right.data()
				|| left.size() == 0
				|| left.compare(right) == 0);
	}

	DEKAF2_CONSTEXPR_14
	inline bool operator!=(string_view left, string_view right) noexcept
	{
		return !(left == right);
	}

	DEKAF2_CONSTEXPR_14
	inline bool operator< (string_view left, string_view right) noexcept
	{
		string_view::size_type min_size = std::min(left.size(), right.size());
		int r = min_size == 0 ? 0 : left.compare(right);
		return (r < 0) || (r == 0 && left.size() < right.size());
	}

	DEKAF2_CONSTEXPR_14
	inline bool operator> (string_view left, string_view right) noexcept
	{
		return right < left;
	}

	DEKAF2_CONSTEXPR_14
	inline bool operator<=(string_view left, string_view right) noexcept
	{
		return !(left > right);
	}

	DEKAF2_CONSTEXPR_14
	inline bool operator>=(string_view left, string_view right) noexcept
	{
		return !(left < right);
	}

	} // end of namespace stringview
	} // end of namespace detail
	} // end of namespace dekaf2

#endif

#ifdef DEKAF2_SV_NAMESPACE
namespace sv = DEKAF2_SV_NAMESPACE;
#endif
