/*
//-----------------------------------------------------------------------------//
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

#include <cinttypes>
#include <functional>
#include <boost/functional/hash.hpp>
#include <string>
#include "bits/kcppcompat.h"
#include "khash.h"
#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
#include <folly/Range.h>
#else
#include <experimental/string_view>
#endif

#ifndef __linux__
extern void* memrchr(const void* s, int c, size_t n);
#endif

namespace dekaf2 {

class KStringView;

//-----------------------------------------------------------------------------
size_t kFind(
        KStringView haystack,
        KStringView needle,
        size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kRFind(
        KStringView haystack,
        KStringView needle,
        size_t pos = std::string::npos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
constexpr
size_t kFind(
        KStringView haystack,
        const char needle,
        size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kRFind(
        KStringView haystack,
        const char needle,
        size_t pos = std::string::npos);
//-----------------------------------------------------------------------------

namespace detail { namespace stringview {

//-----------------------------------------------------------------------------
size_t kFindFirstOfBool(
        KStringView haystack,
        KStringView needle,
        size_t pos,
        bool bNot);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindLastOfBool(
        KStringView haystack,
        KStringView needle,
        size_t pos,
        bool bNot);
//-----------------------------------------------------------------------------

} } // end of namespace detail::stringview

//-----------------------------------------------------------------------------
size_t kFindFirstOf(
        KStringView haystack,
        KStringView needle,
        size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindFirstNotOf(
        KStringView haystack,
        KStringView needle,
        size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindLastOf(
        KStringView haystack,
        KStringView needle,
        size_t pos = std::string::npos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindLastNotOf(
        KStringView haystack,
        KStringView needle,
        size_t pos = std::string::npos);
//-----------------------------------------------------------------------------

//----------------------------------------------------------------------
constexpr
bool kStartsWith(KStringView sInput, KStringView sPattern);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
constexpr
bool kEndsWith(KStringView sInput, KStringView sPattern);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
bool kContains(KStringView sInput, KStringView sPattern);
//----------------------------------------------------------------------

// forward declarations
class KString;
class KStringViewZ;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KStringView {
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//----------
public:
//----------

#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
	using rep_type               = folly::StringPiece;
#else
	using rep_type               = std::experimental::string_view;
#endif
	using self_type              = KStringView;
	using size_type              = std::size_t;
	using iterator               = rep_type::iterator;
	using const_iterator         = rep_type::iterator;
	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = reverse_iterator;
	using value_type             = rep_type::value_type;
	using difference_type        = rep_type::difference_type;
	using reference              = rep_type::reference;
	using const_reference        = rep_type::reference;
	using pointer                = value_type*;
	using const_pointer          = const pointer;
	using traits_type            = rep_type::traits_type;

	static constexpr size_type npos = std::string::npos;

	//-----------------------------------------------------------------------------
	constexpr
	KStringView() noexcept
	//-----------------------------------------------------------------------------
	{
	}

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
	KStringView(const value_type* s, size_type count) noexcept
	//-----------------------------------------------------------------------------
	    : m_rep(s, count)
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const value_type* s) noexcept
	//-----------------------------------------------------------------------------
	    : m_rep(s)
	{
	}

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(KStringViewZ svz) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KStringView(const KString& svz) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	self_type& operator=(const self_type& other) noexcept = default;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	self_type& operator=(KStringViewZ other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	self_type& operator=(const KString& other);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	self_type& operator=(const value_type* other)
	//-----------------------------------------------------------------------------
	{
		assign(other, strlen(other));
		return *this;
	}

	//-----------------------------------------------------------------------------
	constexpr
	const rep_type& ToRange() const
	//-----------------------------------------------------------------------------
	{
		return m_rep;
	}

	//-----------------------------------------------------------------------------
	constexpr
	rep_type& ToRange()
	//-----------------------------------------------------------------------------
	{
		return m_rep;
	}

	//-----------------------------------------------------------------------------
	constexpr
	operator const rep_type&() const
	//-----------------------------------------------------------------------------
	{
		return ToRange();
	}

	//-----------------------------------------------------------------------------
	constexpr
	operator rep_type&()
	//-----------------------------------------------------------------------------
	{
		return ToRange();
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	void clear()
	//-----------------------------------------------------------------------------
	{
#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
		m_rep.clear();
#else
		m_rep.remove_prefix(size());
#endif
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	void assign(const_iterator start, size_type size)
	//-----------------------------------------------------------------------------
	{
		assign(start, start + size);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	void assign(const_iterator start, const_iterator end)
	//-----------------------------------------------------------------------------
	{
#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
		m_rep.assign(start, end);
#else
		m_rep = self_type(start, static_cast<size_type>(end - start));
#endif
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	void reset(const_iterator start, size_type size)
	//-----------------------------------------------------------------------------
	{
		assign(start, size);
	}

	//-----------------------------------------------------------------------------
	constexpr
	size_type max_size() const
	//-----------------------------------------------------------------------------
	{
		return npos - 1;
	}

	//-----------------------------------------------------------------------------
	constexpr
	size_type size() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.size();
	}

	//-----------------------------------------------------------------------------
	constexpr
	size_type length() const
	//-----------------------------------------------------------------------------
	{
		return size();
	}

	//-----------------------------------------------------------------------------
	constexpr
	bool empty() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.empty();
	}

	//-----------------------------------------------------------------------------
	constexpr
	const_iterator data() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.data();
	}

	//-----------------------------------------------------------------------------
	constexpr
	iterator begin() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.begin();
	}

	//-----------------------------------------------------------------------------
	constexpr
	iterator end() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.end();
	}

	//-----------------------------------------------------------------------------
	reverse_iterator rbegin() const
	//-----------------------------------------------------------------------------
	{
		return reverse_iterator(end());
	}

	//-----------------------------------------------------------------------------
	reverse_iterator rend() const
	//-----------------------------------------------------------------------------
	{
		return reverse_iterator(begin());
	}

	//-----------------------------------------------------------------------------
	constexpr
	const_iterator cbegin() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.cbegin();
	}

	//-----------------------------------------------------------------------------
	constexpr
	const_iterator cend() const
	//-----------------------------------------------------------------------------
	{
		return m_rep.cend();
	}

	//-----------------------------------------------------------------------------
	const_reverse_iterator crbegin() const
	//-----------------------------------------------------------------------------
	{
		return const_reverse_iterator(cend());
	}

	//-----------------------------------------------------------------------------
	const_reverse_iterator crend() const
	//-----------------------------------------------------------------------------
	{
		return const_reverse_iterator(cbegin());
	}

	//-----------------------------------------------------------------------------
	constexpr
	const value_type& front() const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(empty())
		{
			Warn("front() is not available");
			return s_0ch;
		}
		return m_rep.front();
	}

	//-----------------------------------------------------------------------------
	constexpr
	const value_type& back() const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(empty())
		{
			Warn("back() is not available");
			return s_0ch;
		}
		return m_rep.back();
	}

	//-----------------------------------------------------------------------------
	constexpr
	int compare(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		// uses __builtin_memcmp(), OK
		return m_rep.compare(other);
	}

	//-----------------------------------------------------------------------------
	constexpr
	int compare(size_type pos1, size_type count1,
	            self_type other) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(other);
	}

	//-----------------------------------------------------------------------------
	constexpr
	int compare(size_type pos1, size_type count1,
	            self_type other,
	            size_type pos2, size_type count2) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(other.substr(pos2, count2));
	}

	//-----------------------------------------------------------------------------
	constexpr
	int compare(const const_iterator str) const
	//-----------------------------------------------------------------------------
	{
		return compare(self_type(str));
	}

	//-----------------------------------------------------------------------------
	constexpr
	int compare(size_type pos1, size_type count1,
	            const const_iterator str) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(self_type(str));
	}

	//-----------------------------------------------------------------------------
	constexpr
	int compare(size_type pos1, size_type count1,
	            const const_iterator str, size_type count2) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(self_type(str, count2));
	}

	//-----------------------------------------------------------------------------
	size_type copy(iterator dest, size_type count, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	constexpr
	const value_type& operator[](size_t index) const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(index >= size())
		{
			Warn("Index access out of range");
			return s_0ch;
		}
		return m_rep[index];
	}

	//-----------------------------------------------------------------------------
	constexpr
	const value_type& at(size_t index) const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(index >= size())
		{
			Warn("Index access out of range");
			return s_0ch;
		}
		return m_rep[index];
	}

	//-----------------------------------------------------------------------------
	constexpr
	self_type substr(size_type pos = 0, size_type count = npos) const
	//-----------------------------------------------------------------------------
	{
		if (pos > size())
		{
			Warn("pos > size()");
			pos = size();
		}
		return self_type(begin() + pos, std::min(count, size() - pos));
	}

	//-----------------------------------------------------------------------------
	constexpr
	void remove_prefix(size_type n)
	//-----------------------------------------------------------------------------
	{
		if (n > size())
		{
			Warn("n > size()");
			n = size();
		}
		unchecked_remove_prefix(n);
	}

	//-----------------------------------------------------------------------------
	constexpr
	void remove_suffix(size_type n)
	//-----------------------------------------------------------------------------
	{
		if (n > size())
		{
			Warn("n > size()");
			n = size();
		}
		unchecked_remove_suffix(n);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	bool remove_prefix(self_type other)
	//-----------------------------------------------------------------------------
	{
		if (StartsWith(other))
		{
			unchecked_remove_prefix(other.size());
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	bool remove_suffix(self_type other)
	//-----------------------------------------------------------------------------
	{
		if (EndsWith(other))
		{
			unchecked_remove_suffix(other.size());
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	bool StartsWith(self_type other)
	//-----------------------------------------------------------------------------
	{
		return kStartsWith(*this, other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	constexpr
	bool EndsWith(self_type other)
	//-----------------------------------------------------------------------------
	{
		return kEndsWith(*this, other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	bool Contains(self_type other)
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, other);
	}

	//-----------------------------------------------------------------------------
	void swap(self_type& other)
	//-----------------------------------------------------------------------------
	{
		std::swap(m_rep, other.m_rep);
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: output the hash value of instance by calling std::hash() for the type
	std::size_t Hash() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// nonstandard: emulate erase if range is at begin or end
	self_type& erase(size_type pos = 0, size_type n = npos);
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
	size_type find(const self_type str, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFind(*this, str, pos);
	}

	//-----------------------------------------------------------------------------
	constexpr
	size_type find(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFind(*this, ch, pos);
	}

	//-----------------------------------------------------------------------------
	size_type rfind(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kRFind(*this, ch, pos);
	}

	//-----------------------------------------------------------------------------
	size_type rfind(self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kRFind(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_of(self_type sv, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindFirstOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_of(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find(ch, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_of(self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindLastOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_of(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return rfind(ch, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_not_of(self_type sv, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindFirstNotOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_not_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_not_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_first_not_of(value_type ch, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_first_not_of(self_type(&ch, 1), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_not_of(self_type sv, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFindLastNotOf(*this, sv, pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_not_of(const value_type* s, size_type pos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(self_type(s), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_not_of(const value_type* s, size_type pos, size_type count) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(self_type(s, count), pos);
	}

	//-----------------------------------------------------------------------------
	size_type find_last_not_of(value_type ch, size_type pos = npos) const noexcept
	//-----------------------------------------------------------------------------
	{
		return find_last_not_of(self_type(&ch, 1), pos);
	}

	/// is string one of the values in sHaystack, delimited by iDelim?
	bool In (KStringView sHaystack, value_type iDelim=',') const;

	// conversions

	//-----------------------------------------------------------------------------
	bool Bool() const noexcept
	//-----------------------------------------------------------------------------
	{
		return Int16() != 0;
	}

	//-----------------------------------------------------------------------------
	int16_t Int16(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	uint16_t UInt16(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	int32_t Int32(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	uint32_t UInt32(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	int64_t Int64(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	uint64_t UInt64(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_INT128
	//-----------------------------------------------------------------------------
	int128_t Int128(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	uint128_t UInt128(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	float Float() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	double Double() const noexcept;
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	DEKAF2_ALWAYS_INLINE
	constexpr
	void unchecked_remove_prefix(size_type n)
	//-----------------------------------------------------------------------------
	{
#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
		m_rep.uncheckedAdvance(n);
#else
		m_rep.remove_prefix(n);
#endif
	}

	//-----------------------------------------------------------------------------
	DEKAF2_ALWAYS_INLINE
	constexpr
	void unchecked_remove_suffix(size_type n)
	//-----------------------------------------------------------------------------
	{
#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
		m_rep.uncheckedSubtract(n);
#else
		m_rep.remove_suffix(n);
#endif
	}

	//-----------------------------------------------------------------------------
	void Warn(KStringView sWhat) const;
	//-----------------------------------------------------------------------------

	rep_type m_rep;

	static constexpr value_type s_0ch = '\0';

};

using KStringViewPair = std::pair<KStringView, KStringView>;


//-----------------------------------------------------------------------------
constexpr
inline bool operator==(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	KStringView::size_type len = left.size();
	if (len != right.size())
	{
		return false;
	}
	return left.data() == right.data()
	        || len == 0
	        || left.compare(right) == 0;
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator==(const KStringView::value_type* left, KStringView right)
//-----------------------------------------------------------------------------
{
	return KStringView(left) == right;
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator==(KStringView left, const KStringView::value_type* right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(const KStringView::value_type* left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator!=(KStringView left, const KStringView::value_type* right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator<(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	KStringView::size_type min_size = std::min(left.size(), right.size());
	int r = min_size == 0 ? 0 : left.compare(right);
	return (r < 0) || (r == 0 && left.size() < right.size());
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator>(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right < left;
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator<=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left > right);
}

//-----------------------------------------------------------------------------
constexpr
inline bool operator>=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left < right);
}


//-----------------------------------------------------------------------------
inline
constexpr
size_t kFind(
        KStringView haystack,
        const char needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	// we keep this inlined as then the compiler can evaluate const expressions
	// (memchr() is actually a compiler-builtin with gcc)
	if (DEKAF2_UNLIKELY(pos > haystack.size()))
	{
		return KStringView::npos;
	}
	// TODO compiler conditionals
	auto ret = static_cast<const char*>(__builtin_memchr(haystack.data()+pos, needle, haystack.size()-pos));
	if (DEKAF2_UNLIKELY(ret == nullptr))
	{
		return KStringView::npos;
	}
	else
	{
		return static_cast<size_t>(ret - haystack.data());
	}
#else
	return static_cast<KStringView::rep_type>(haystack).find(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kRFind(
        KStringView haystack,
        const char needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	&& !defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	if (DEKAF2_UNLIKELY(pos >= haystack.size()))
	{
		pos = haystack.size();
	}
	else
	{
		++pos;
	}
	haystack.remove_suffix(haystack.size() - pos);
	return static_cast<KStringView::rep_type>(haystack).rfind(needle);
#elif !defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	return static_cast<KStringView::rep_type>(haystack).rfind(needle, pos);
#else
	// we keep this inlined as then the compiler can evaluate const expressions
	// (memrchr() is actually a compiler-builtin with gcc)
	if (DEKAF2_UNLIKELY(pos >= haystack.size()))
	{
		pos = haystack.size();
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
	else
	{
		return static_cast<size_t>(found - haystack.data());
	}
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindFirstOf(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	return detail::stringview::kFindFirstOfBool(haystack, needle, pos, false);
#else
	return static_cast<KStringView::rep_type>(haystack).find_first_of(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindFirstNotOf(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) || defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
	return detail::stringview::kFindFirstOfBool(haystack, needle, pos, true);
#else
	return static_cast<KStringView::rep_type>(haystack).find_first_not_of(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindLastOf(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) || defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
	return detail::stringview::kFindLastOfBool(haystack, needle, pos, false);
#else
	return static_cast<KStringView::rep_type>(haystack).find_last_of(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
inline
size_t kFindLastNotOf(
        KStringView haystack,
        KStringView needle,
        size_t pos)
//-----------------------------------------------------------------------------
{
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) || defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
	return detail::stringview::kFindLastOfBool(haystack, needle, pos, true);
#else
	return static_cast<KStringView::rep_type>(haystack).find_last_not_of(needle, pos);
#endif
}

//-----------------------------------------------------------------------------
/// Find delimiter chars prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter chars prefixed by odd number of escapes.
size_t kFindFirstOfUnescaped(KStringView haystack,
                             KStringView needle,
                             KStringView::value_type chEscape,
                             KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Find delimiter char prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter charsprefixed by odd number of escapes.
size_t kFindUnescaped(KStringView haystack,
                      KStringView::value_type needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//----------------------------------------------------------------------
inline
constexpr
bool kStartsWith(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.size() < sPattern.size()))
	{
		return false;
	}

	return !std::memcmp(sInput.data(), sPattern.data(), sPattern.size());

} // kStartsWith

//----------------------------------------------------------------------
inline
constexpr
bool kEndsWith(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.size() < sPattern.size()))
	{
		return false;
	}

	return !std::memcmp(sInput.data() + sInput.size() - sPattern.size(), sPattern.data(), sPattern.size());

} // kEndsWith

//----------------------------------------------------------------------
inline
bool kContains(KStringView sInput, KStringView sPattern)
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.size() < sPattern.size()))
	{
		return false;
	}

	return kFind(sInput, sPattern) != KStringView::npos;

} // kContains

inline namespace literals {

	/// provide a string literal for KStringView
	constexpr dekaf2::KStringView operator"" _ksv(const char *data, std::size_t size)
	{
		return {data, size};
	}

} // namespace literals

//-----------------------------------------------------------------------------
inline std::ostream& operator <<(std::ostream& stream, KStringView str)
//-----------------------------------------------------------------------------
{
	stream.write(str.data(), str.size());
	return stream;
}

} // end of namespace dekaf2

namespace std
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a std::hash for KStringView
	template<>
	struct hash<dekaf2::KStringView>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		typedef dekaf2::KStringView argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type s) const
		{
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
		}
	};

} // namespace std

namespace boost
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a boost::hash for KStringView
	template<>
	struct hash<dekaf2::KStringView>
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		typedef dekaf2::KStringView argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type s) const
		{
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
		}
	};

} // namespace boost

//----------------------------------------------------------------------
inline std::size_t dekaf2::KStringView::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<dekaf2::KStringView>()(*this);
}

#include "bits/kstringviewz.h"

//-----------------------------------------------------------------------------
inline
constexpr
dekaf2::KStringView::KStringView(KStringViewZ svz) noexcept
//-----------------------------------------------------------------------------
: KStringView(svz.data(), svz.size())
{
}

//-----------------------------------------------------------------------------
inline
constexpr
dekaf2::KStringView& dekaf2::KStringView::operator=(dekaf2::KStringViewZ other)
//-----------------------------------------------------------------------------
{
	assign(other.begin(), other.end());
	return *this;
}

#include "kstring.h"

//-----------------------------------------------------------------------------
inline
dekaf2::KStringView::KStringView(const KString& str) noexcept
//-----------------------------------------------------------------------------
: KStringView(str.data(), str.size())
{
}

//-----------------------------------------------------------------------------
inline
dekaf2::KStringView& dekaf2::KStringView::operator=(const dekaf2::KString& other)
//-----------------------------------------------------------------------------
{
	assign(other.data(), other.size());
	return *this;
}

