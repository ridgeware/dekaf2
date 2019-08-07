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
#include <cstring>

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

#ifdef DEKAF2_SV_NAMESPACE
namespace sv = DEKAF2_SV_NAMESPACE;
#endif

#if defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW) \
	|| (!defined(DEKAF2_HAS_STD_STRING_VIEW) \
		&& !defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW))

	/// tiny but nearly complete string_view implementation - it only does not have rfind() nor find_first/last_(not)_of() (but those are supplied through KStringView)

	#define DEKAF2_HAS_OWN_STRING_VIEW 1

	namespace dekaf2 {
	namespace detail {
	namespace stringview {

	template<typename CharT, typename Traits = std::char_traits<CharT>>
	class basic_string_view {
	public:
		using traits_type            = Traits;
		using value_type             = CharT;
		using size_type              = std::size_t;
		using pointer                = CharT*;
		using const_pointer          = const CharT*;
		using reference              = CharT&;
		using const_reference        = const CharT&;
		using const_iterator         = const_pointer;
		using iterator               = const_iterator;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using reverse_iterator       = const_reverse_iterator;
		using difference_type        = typename std::iterator_traits<const_pointer>::difference_type;

	private:
		static constexpr size_t CharSize = sizeof(CharT);

	public:

		static constexpr size_type npos = size_type(-1);

		constexpr
		basic_string_view() noexcept
		: m_pszString(&s_chEmpty)
		, m_iSize(0)
		{}

		constexpr
		basic_string_view(const basic_string_view& other) noexcept = default;

		basic_string_view(const std::string& strStr) noexcept
		: m_pszString(strStr.c_str())
		, m_iSize(strStr.size())
		{}

		constexpr
		basic_string_view(const CharT* pszStr) noexcept
		: m_pszString(pszStr ? pszStr : &s_chEmpty)
		, m_iSize(constexpr_strlen(pszStr))
		{}

		constexpr
		basic_string_view(const CharT* pszStr, size_type iSize) noexcept
		: m_pszString(pszStr ? pszStr : &s_chEmpty)
		, m_iSize(pszStr ? iSize : 0)
		{}

		DEKAF2_CONSTEXPR_14
		basic_string_view& operator=(const basic_string_view& other) noexcept = default;

		DEKAF2_CONSTEXPR_14
		void clear() noexcept
		{
			m_pszString = &s_chEmpty;
			m_iSize = 0;
		}

		constexpr
		size_type max_size() const noexcept
		{
			return size_type(-1) - 1;
		}

		constexpr
		size_type size() const noexcept
		{
			return m_iSize;
		}

		constexpr
		size_type length() const noexcept
		{
			return size();
		}

		constexpr
		bool empty() const noexcept
		{
			return !size();
		}

		constexpr
		iterator begin() const noexcept
		{
			return m_pszString;
		}

		constexpr
		iterator end() const noexcept
		{
			return m_pszString + m_iSize;
		}

		constexpr
		iterator cbegin() const noexcept
		{
			return begin();
		}

		constexpr
		iterator cend() const noexcept
		{
			return end();
		}

		DEKAF2_CONSTEXPR_17
		reverse_iterator rbegin() const
		{
			return reverse_iterator(end());
		}

		DEKAF2_CONSTEXPR_17
		reverse_iterator rend() const
		{
			return reverse_iterator(begin());
		}

		DEKAF2_CONSTEXPR_17
		const_reverse_iterator crbegin() const
		{
			return const_reverse_iterator(cend());
		}

		DEKAF2_CONSTEXPR_17
		const_reverse_iterator crend() const
		{
			return const_reverse_iterator(cbegin());
		}

		constexpr
		const_reference front() const noexcept
		{
			return *begin();
		}

		constexpr
		const_reference back() const noexcept
		{
			return *(end() - 1);
		}

		constexpr
		const_pointer data() const noexcept
		{
			return begin();
		}

		constexpr
		const_reference operator[](size_type pos) const noexcept
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

		// non-standard
		DEKAF2_CONSTEXPR_14
		void unchecked_remove_prefix(size_type n) noexcept
		{
			m_pszString += n;
			m_iSize -= n;
		}

		// non-standard
		DEKAF2_CONSTEXPR_14
		void unchecked_remove_suffix(size_type n) noexcept
		{
			m_iSize -= n;
		}

		DEKAF2_CONSTEXPR_14
		void remove_prefix(size_type n) noexcept
		{
			if (n > m_iSize)
			{
				n = m_iSize;
			}
			unchecked_remove_prefix(n);
		}

		DEKAF2_CONSTEXPR_14
		void remove_suffix(size_type n) noexcept
		{
			if (n > m_iSize)
			{
				n = m_iSize;
			}
			unchecked_remove_suffix(n);
		}

		DEKAF2_CONSTEXPR_14
		void swap(basic_string_view& other) noexcept
		{
			using std::swap;
			swap(*this, other);
		}

		size_type copy(CharT* s, size_type n, size_type pos = 0) const
		{
			if (pos > size())
			{
				throw std::out_of_range("string_view::copy");
			}
			size_type rlen = std::min(n, size() - pos);
			traits_type::copy(s, data() + pos, rlen);
			return rlen;
		}

		DEKAF2_CONSTEXPR_14
		basic_string_view substr(size_type pos = 0, size_type count = npos) const noexcept
		{
			return basic_string_view(m_pszString + pos, std::min(count, size() - pos));
		}

		DEKAF2_CONSTEXPR_17
		int compare(basic_string_view other) const noexcept
		{
			auto cmp = traits_type::compare(data(), other.data(), std::min(size(), other.size()));
			return (cmp) ? cmp : size() - other.size();
		}

		// this find() is constexpr only if it has a needle size of less than 2
		// for any other size another solution needs to be found still
		DEKAF2_CONSTEXPR_17 inline
		size_type find(basic_string_view needle, size_type pos = 0) const noexcept
		{
			if (needle.size() == 1)
			{
				return find(needle.front(), pos);
			}
			if (pos >= size() || needle.empty() || needle.size() > (size() - pos))
			{
				return npos;
			}
			auto iNeedleBytes = needle.size() * CharSize;
			auto pNeedleBytes = static_cast<const char*>(needle.data());
			auto pHaystackBytes = static_cast<const char*>(data());
#ifdef __GNUC__
			auto pFound = static_cast<const char*>(::memmem(pHaystackBytes + pos * CharSize,
														   (size() - pos) * CharSize,
														   pNeedleBytes,
														   iNeedleBytes));
			return (pFound) ? static_cast<size_t>(pFound - pHaystackBytes) / CharSize : npos;
#else
			for(;;)
			{
				auto pFound = static_cast<const char*>(::memchr(pHaystackBytes + pos * CharSize,
															    pNeedleBytes[0],
															    ((size() - pos - needle.size()) + 1) * CharSize));
				if (DEKAF2_UNLIKELY(!pFound))
				{
					return KStringView::npos;
				}

				pos = static_cast<size_t>(pFound - pHaystackBytes) / CharSize;

				// due to aligned loads it is faster to compare the full needle again
				if (std::memcmp(pHaystackBytes + pos * CharSize,
								pNeedleBytes,
								iNeedleBytes) == 0)
				{
					return pos;
				}

				++pos;
			}
#endif
		}

		DEKAF2_CONSTEXPR_17 inline
		size_type find(CharT ch, size_type pos = 0) const noexcept
		{
			if (pos >= size())
			{
				return npos;
			}
			auto found = traits_type::find(data() + pos, size() - pos, ch);
			return (found) ? static_cast<size_t>(found - data()) : npos;
		}

		DEKAF2_CONSTEXPR_17 inline
		size_type find(const CharT* s, size_type pos, size_type count) const noexcept
		{
			return find(basic_string_view(s, count), pos);
		}

		DEKAF2_CONSTEXPR_17 inline
		size_type find(const CharT* s, size_type pos) const noexcept
		{
			return find(basic_string_view(s), pos);
		}

		DEKAF2_CONSTEXPR_14
		bool starts_with(basic_string_view sv) const noexcept
		{
			return size() >= sv.size() && !compare(0, sv.size(), sv);
		}

		DEKAF2_CONSTEXPR_14
		bool starts_with(CharT ch) const noexcept
		{
			return !empty() && traits_type::eq(front(), ch);
		}

		DEKAF2_CONSTEXPR_14
		bool starts_with(const CharT* s) const noexcept
		{
			return starts_with(basic_string_view(s));
		}

		DEKAF2_CONSTEXPR_14
		bool ends_with(basic_string_view sv) const noexcept
		{
			return size() >= sv.size() && !compare(size() - sv.size(), npos, sv);
		}

		DEKAF2_CONSTEXPR_14
		bool ends_with(CharT ch) const noexcept
		{
			return !empty() && traits_type::eq(back(), ch);
		}

		DEKAF2_CONSTEXPR_14
		bool ends_with(const CharT* s) const noexcept
		{
			return ends_with(basic_string_view(s));
		}

		static constexpr DEKAF2_ALWAYS_INLINE
		size_type constexpr_strlen(const CharT* s) noexcept
		{
	#ifdef DEKAF2_HAS_CPP_17
			return s ? traits_type::length(s) : 0;
	#else
		#if defined(__clang__)
			return s ? __builtin_strlen(s) : 0;
		#else
			#define DEKAF2_NEED_LOCAL_CONSTEXPR_STRLEN 1
			return s ? local_constexpr_strlen(s) : 0;
		#endif
	#endif
		}

	private:

#ifdef DEKAF2_NEED_LOCAL_CONSTEXPR_STRLEN
	#if (DEKAF2_HAS_CPP_14)
		static constexpr DEKAF2_ALWAYS_INLINE
		size_type local_constexpr_strlen(const CharT* s) noexcept
		{
			const CharT* start = s;
			while (*s++);
			return s - start - 1;
		}
	#else
		static constexpr DEKAF2_ALWAYS_INLINE
		size_type local_constexpr_strlen_1(const CharT* s, size_type len) noexcept
		{
			return *s == 0 ? len : local_constexpr_strlen_1(s + 1, len + 1);
		}

		static constexpr inline
		size_t local_constexpr_strlen(const CharT* s) noexcept
		{
			return local_constexpr_strlen_1(s, 0);
		}
	#endif
#endif
		static constexpr CharT s_chEmpty { 0 };
		const CharT* m_pszString;
		std::size_t m_iSize;

	}; // string_view

#ifdef DEKAF2_REPEAT_CONSTEXPR_VARIABLE
	// defines the template's static const for c++ < 17
	template<typename CharT, typename Traits>
	const CharT basic_string_view<CharT, Traits>::s_chEmpty;
#endif

	template<typename CharT, typename Traits>
	DEKAF2_CONSTEXPR_17
	inline bool operator==(basic_string_view<CharT, Traits> left, basic_string_view<CharT, Traits> right) noexcept
	{
		return left.size() == right.size() && left.compare(right) == 0;
	}

	template<typename CharT, typename Traits>
	DEKAF2_CONSTEXPR_17
	inline bool operator!=(basic_string_view<CharT, Traits> left, basic_string_view<CharT, Traits> right) noexcept
	{
		return !(left == right);
	}

	template<typename CharT, typename Traits>
	DEKAF2_CONSTEXPR_17
	inline bool operator< (basic_string_view<CharT, Traits> left, basic_string_view<CharT, Traits> right) noexcept
	{
		return left.compare(right) < 0;
	}

	template<typename CharT, typename Traits>
	DEKAF2_CONSTEXPR_17
	inline bool operator> (basic_string_view<CharT, Traits> left, basic_string_view<CharT, Traits> right) noexcept
	{
		return right < left;
	}

	template<typename CharT, typename Traits>
	DEKAF2_CONSTEXPR_17
	inline bool operator<=(basic_string_view<CharT, Traits> left, basic_string_view<CharT, Traits> right) noexcept
	{
		return !(left > right);
	}

	template<typename CharT, typename Traits>
	DEKAF2_CONSTEXPR_17
	inline bool operator>=(basic_string_view<CharT, Traits> left, basic_string_view<CharT, Traits> right) noexcept
	{
		return !(left < right);
	}

	using string_view    = basic_string_view<char>;
	using wstring_view   = basic_string_view<wchar_t>;
#ifdef DEKAF2_HAS_CPP_20
	using u8string_view  = basic_string_view<char8_t>;
#else
	using u8string_view  = basic_string_view<char>;
#endif
	using u16string_view = basic_string_view<char16_t>;
	using u32string_view = basic_string_view<char32_t>;

	} // end of namespace stringview
	} // end of namespace detail
	} // end of namespace dekaf2

#endif


