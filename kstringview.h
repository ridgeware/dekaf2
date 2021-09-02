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

#include <cinttypes>
#include <functional>
#include <boost/functional/hash.hpp>
#include <string>
#include <vector>
#include "bits/kcppcompat.h"
#include "bits/kstring_view.h"
#include "bits/khash.h"
#include "kutf8.h"
#include <fmt/format.h>

#if !defined(DEKAF2_HAS_STD_STRING_VIEW) \
	&& !defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	&& !defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	// we have to use our own string_view if we do not have C++17..
	#define DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW 1
#endif

#ifdef DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW
	#include <folly/Range.h>
#endif

#ifndef __linux__
	extern void* memrchr(const void* s, int c, size_t n);
#endif

// older gcc versions have the cpp17 flag, but their libstdc++ does not
// support the constexpr reverse iterators
#define DEKAF2_CONSTEXPR_REVERSE_ITERATORS
#if defined(DEKAF2_HAS_CPP_17)
	#if (DEKAF2_NO_GCC || DEKAF2_GCC_VERSION >= 80000)
		#undef DEKAF2_CONSTEXPR_REVERSE_ITERATORS
		#define DEKAF2_CONSTEXPR_REVERSE_ITERATORS constexpr
	#endif
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
DEKAF2_CONSTEXPR_14
size_t kFind(
        KStringView haystack,
        char needle,
        size_t pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kRFind(
        KStringView haystack,
        char needle,
        size_t pos = std::string::npos);
//-----------------------------------------------------------------------------

namespace detail { namespace stringview {

//-----------------------------------------------------------------------------
size_t kFindFirstOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindFirstNotOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindLastOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
size_t kFindLastNotOfInt(
        KStringView haystack,
        KStringView needle,
        size_t pos);
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
DEKAF2_CONSTEXPR_14
bool kStartsWith(KStringView sInput, KStringView sPattern) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool kEndsWith(KStringView sInput, KStringView sPattern) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
bool kContains(KStringView sInput, KStringView sPattern) noexcept;
//----------------------------------------------------------------------

//----------------------------------------------------------------------
bool kContains(KStringView sInput, char ch) noexcept;
//----------------------------------------------------------------------

// forward declarations
class KString;
class KStringViewZ;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf2's own string view class - a wrapper around std::string_view or
/// folly::StringPiece, or our own implementation. Handles most errors without
/// throwing and speeds up searching up to 50 times compared to std::string_view
/// implementations.
class DEKAF2_GSL_POINTER() KStringView {
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//----------
public:
//----------

#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
	using rep_type               = folly::StringPiece;
#elif defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	using rep_type               = dekaf2::detail::stringview::string_view;
#else
	using rep_type               = DEKAF2_SV_NAMESPACE::string_view;
#endif
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

	static constexpr size_type npos = std::string::npos;

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

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	//-----------------------------------------------------------------------------
	constexpr
	KStringView(sv::string_view str) noexcept
	//-----------------------------------------------------------------------------
		: m_rep(str.data(), str.size())
	{
	}
#endif

	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const value_type* s, size_type count) noexcept
	//-----------------------------------------------------------------------------
#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
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
#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	: m_rep(s)
#else
	// std::string_view in clang and gnu libc++ is not resilient to nullptr assignment
	// - therefore we protect it
	: m_rep(s ? s : &s_0ch)
#endif
	{
	}

#ifdef _MSC_VER
	// MSC refuses base class conversion if we pass the extended class by value..
	//-----------------------------------------------------------------------------
	constexpr
	KStringView(const KStringViewZ& svz) noexcept;
	//-----------------------------------------------------------------------------
#else
	//-----------------------------------------------------------------------------
	constexpr
	KStringView(KStringViewZ svz) noexcept;
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	KStringView(const KString& str) noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(const self_type& other) noexcept = default;
	//-----------------------------------------------------------------------------

#ifdef _MSC_VER
	// MSC refuses base class conversion if we pass the extended class by value..
	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(const KStringViewZ& other);
	//-----------------------------------------------------------------------------
#else
	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(KStringViewZ other);
	//-----------------------------------------------------------------------------
#endif

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

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self& operator=(sv::string_view other)
	//-----------------------------------------------------------------------------
	{
		*this = self_type(other);
		return *this;
	}
#endif

	//-----------------------------------------------------------------------------
	constexpr
	operator const rep_type&() const
	//-----------------------------------------------------------------------------
	{
		return m_rep;
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
	DEKAF2_CONSTEXPR_14
	void clear()
	//-----------------------------------------------------------------------------
	{
#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
		m_rep.clear();
#else
		m_rep.remove_prefix(size());
#endif
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	DEKAF2_CONSTEXPR_14
	void assign(const value_type* start, size_type size)
	//-----------------------------------------------------------------------------
	{
		assign(start, start + size);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	DEKAF2_CONSTEXPR_14
	void assign(const value_type* start, const value_type* end)
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
	DEKAF2_CONSTEXPR_14
	void reset(const value_type* start, size_type size)
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
	/// test if the string is non-empty
	constexpr
	explicit operator bool() const
	//-----------------------------------------------------------------------------
	{
		return !empty();
	}

	//-----------------------------------------------------------------------------
	constexpr
	const value_type* data() const
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
	DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	reverse_iterator rbegin() const
	//-----------------------------------------------------------------------------
	{
		return reverse_iterator(end());
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_REVERSE_ITERATORS
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
	DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	const_reverse_iterator crbegin() const
	//-----------------------------------------------------------------------------
	{
		return const_reverse_iterator(cend());
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_REVERSE_ITERATORS
	const_reverse_iterator crend() const
	//-----------------------------------------------------------------------------
	{
		return const_reverse_iterator(cbegin());
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	const value_type& front() const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(empty())
		{
			Warn(DEKAF2_FUNCTION_NAME, "front() is not available");
			return s_0ch;
		}
		return m_rep.front();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	const value_type& back() const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(empty())
		{
			Warn(DEKAF2_FUNCTION_NAME, "back() is not available");
			return s_0ch;
		}
		return m_rep.back();
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_17
	int compare(const self_type& other) const
	//-----------------------------------------------------------------------------
	{
		// uses __builtin_memcmp(), OK
		return m_rep.compare(other);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            self_type other) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(other);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            self_type other,
	            size_type pos2, size_type count2) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(other.substr(pos2, count2));
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_17
	int compare(const value_type* str) const
	//-----------------------------------------------------------------------------
	{
		return compare(self_type(str));
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            const value_type* str) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(self_type(str));
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_17
	int compare(size_type pos1, size_type count1,
	            const value_type* str, size_type count2) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos1, count1).compare(self_type(str, count2));
	}

	//-----------------------------------------------------------------------------
	size_type copy(value_type* dest, size_type count, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	const value_type& operator[](size_t index) const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(index >= size())
		{
			Warn(DEKAF2_FUNCTION_NAME, "Index access out of range");
			return s_0ch;
		}
		return m_rep[index];
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	const value_type& at(size_t index) const
	//-----------------------------------------------------------------------------
	{
		if DEKAF2_UNLIKELY(index >= size())
		{
			Warn(DEKAF2_FUNCTION_NAME, "Index access out of range");
			return s_0ch;
		}
		return m_rep[index];
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	self_type substr(size_type pos = 0, size_type count = npos) const
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(pos > size()))
		{
			Warn(DEKAF2_FUNCTION_NAME, "pos > size()");
			pos = size();
		}
		return self_type(data() + pos, std::min(count, size() - pos));
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a sub-view of the current view
	DEKAF2_CONSTEXPR_14
	self_type ToView(size_type pos = 0, size_type count = npos) const
	//-----------------------------------------------------------------------------
	{
		return substr(pos, count);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	void remove_prefix(size_type n)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(n > size()))
		{
			Warn(DEKAF2_FUNCTION_NAME, "n > size()");
			n = size();
		}
		unchecked_remove_prefix(n);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	void remove_suffix(size_type n)
	//-----------------------------------------------------------------------------
	{
		if (DEKAF2_UNLIKELY(n > size()))
		{
			Warn(DEKAF2_FUNCTION_NAME, "n > size()");
			n = size();
		}
		unchecked_remove_suffix(n);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	DEKAF2_CONSTEXPR_14
	bool remove_prefix(self_type other)
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
	DEKAF2_CONSTEXPR_14
	bool remove_suffix(self_type other)
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
	/// append other view to this. Views must overlap or be adjacent, but other has to be to the right of this
	DEKAF2_CONSTEXPR_14
	bool append(self_type other)
	//-----------------------------------------------------------------------------
	{
		if (data() > other.data())
		{
			Warn(DEKAF2_FUNCTION_NAME, "cannot append, other view starts before this");
			return false;
		}

		if (data() + size() < other.data())
		{
			Warn(DEKAF2_FUNCTION_NAME, "non-adjacent views cannot be merged");
			return false;
		}

		if (data() + size() >= other.data() + other.size())
		{
			Warn(DEKAF2_FUNCTION_NAME, "right view is a subset of left");
			return false;
		}

		assign(data(), (other.data() - data()) + other.size());

		return true;
	}

	//--------------------------------------------------------------------------------
	// nonstandard
	/// merge another view with this, regardless from which side and how much overlap..
	DEKAF2_CONSTEXPR_14
	bool Merge(self_type other)
	//--------------------------------------------------------------------------------
	{
		if (data() > other.data())
		{
			if (other.data() + other.size() < data())
			{
				Warn(DEKAF2_FUNCTION_NAME, "non-adjacent views cannot be merged");
				return false;
			}

			swap(other);
		}

		// now it is assured that this view starts before or at the same position as other

		if (data() + size() < other.data() )
		{
			Warn(DEKAF2_FUNCTION_NAME, "non-adjacent views cannot be merged");
			return false;
		}

		assign(data(), (other.data() - data()) + other.size());

		return true;
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string start with sPattern?
	DEKAF2_CONSTEXPR_14
	bool starts_with(self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kStartsWith(*this, other);
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string start with ch?
	DEKAF2_CONSTEXPR_14
	bool starts_with(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return starts_with(self_type(&ch, 1));
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string end with sPattern?
	DEKAF2_CONSTEXPR_14
	bool ends_with(self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kEndsWith(*this, other);
	}

	//-----------------------------------------------------------------------------
	// std::C++20
	/// does the string end with ch?
	DEKAF2_CONSTEXPR_14
	bool ends_with(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return ends_with(self_type(&ch, 1));
	}

	//-----------------------------------------------------------------------------
	// std::C++23
	/// does the string contain the sPattern?
	bool contains(self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, other);
	}

	//-----------------------------------------------------------------------------
	// std::C++23
	/// does the string contain the ch?
	bool contains(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, ch);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string start with sPattern? (Now deprecated, replace by starts_with())
	DEKAF2_CONSTEXPR_14
	bool StartsWith(self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return starts_with(other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string end with sPattern? (Now deprecated, replace by ends_with())
	DEKAF2_CONSTEXPR_14
	bool EndsWith(self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return ends_with(other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string contain the sPattern? (Now deprecated, replace by contains())
	bool Contains(self_type other) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, other);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// does the string contain the ch? (Now deprecated, replace by contains())
	bool Contains(value_type ch) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kContains(*this, ch);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in uppercase (UTF8)
	KString ToUpper() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in lowercase (UTF8)
	KString ToLower() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
	KString ToUpperLocale() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	KString ToLowerLocale() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in uppercase assuming ASCII encoding
	KString ToUpperASCII() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns a copy of the string in lowercase assuming ASCII encoding
	KString ToLowerASCII() const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// match with regular expression and return the overall match (group 0)
	KStringView MatchRegex(KStringView sRegEx, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// match with regular expression and return all match groups
	std::vector<KStringView> MatchRegexGroups(KStringView sRegEx, size_type pos = 0) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns leftmost iCount chars of string
	KStringView Left(size_type iCount) const
	//-----------------------------------------------------------------------------
	{
		return substr(0, iCount);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at iStart for iCount chars
	DEKAF2_CONSTEXPR_14
	KStringView Mid(size_type iStart, size_type iCount = npos) const
	//-----------------------------------------------------------------------------
	{
		return substr(iStart, iCount);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns rightmost iCount chars of string
	KStringView Right(size_type iCount) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns leftmost iCount codepoints of string
	DEKAF2_CONSTEXPR_14
	KStringView LeftUTF8(size_type iCount) const
	//-----------------------------------------------------------------------------
	{
		return Unicode::LeftUTF8(*this, iCount);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns substring starting at codepoint iStart for iCount codepoints
	DEKAF2_CONSTEXPR_14
	KStringView MidUTF8(size_type iStart, size_type iCount = npos) const
	//-----------------------------------------------------------------------------
	{
		return Unicode::MidUTF8(*this, iStart, iCount);
	}

	//-----------------------------------------------------------------------------
	// nonstandard
	/// returns rightmost iCount UTF8 codepoints of string
	DEKAF2_CONSTEXPR_14
	KStringView RightUTF8(size_type iCount) const
	//-----------------------------------------------------------------------------
	{
		return Unicode::RightUTF8(*this, iCount);
	}

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
	self& TrimLeft(KStringView sTrim);
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
	self& TrimRight(KStringView sTrim);
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
	self& Trim(KStringView sTrim);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Clip removing sClipAt and everything to its right if found; otherwise do not alter the string
	bool ClipAt(KStringView sClipAt);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(KStringView sClipAtReverse);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	// nonstandard
	/// Splits string into token container using delimiters, trim, and escape. Returned
	/// Container is a sequence, like a vector, or an associative container like a map.
	/// @return a new Container. Default is a std::vector<KStringView>.
	/// @param svDelim a string view of delimiter characters. Defaults to ",".
	/// @param svPairDelim exists only for associative containers: a string view that is used to separate keys and values in the sequence. Defaults to "=".
	/// @param svTrim a string containing chars to remove from token ends. Defaults to " \t\r\n\b".
	/// @param chEscape Escape character for delimiters. Defaults to '\0' (disabled).
	/// @param bCombineDelimiters if true skips consecutive delimiters (an action always
	/// taken for found spaces if defined as delimiter). Defaults to false.
	/// @param bQuotesAreEscapes if true, escape characters and delimiters inside
	/// double quotes are treated as literal chars, and quotes themselves are removed.
	/// No trimming is applied inside the quotes (but outside). The quote has to be the
	/// first character after applied trimming, and trailing content after the closing quote
	/// is not considered part of the token. Defaults to false.
	template<typename T = std::vector<KStringView>, typename... Parms>
	T Split(Parms&&... parms) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void swap(self_type& other)
	//-----------------------------------------------------------------------------
	{
		std::swap(m_rep, other.m_rep);
	}

	//-----------------------------------------------------------------------------
	/// nonstandard: output the hash value of instance by calling std::hash() for the type
	DEKAF2_CONSTEXPR_14
	std::size_t Hash() const;
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
	size_type find(const self_type str, size_type pos = 0) const noexcept
	//-----------------------------------------------------------------------------
	{
		return kFind(*this, str, pos);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
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
	DEKAF2_CONSTEXPR_14
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
	/// returns bool representation of the string:
	/// "true" --> true
	/// "false" --> false
	/// as well as non-0 --> true
	bool Bool() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to int16_t - set bIsHex to true if string is a hex number
	int16_t Int16(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint16_t - set bIsHex to true if string is a hex number
	uint16_t UInt16(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to int32_t - set bIsHex to true if string is a hex number
	int32_t Int32(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint32_t - set bIsHex to true if string is a hex number
	uint32_t UInt32(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to int64_t - set bIsHex to true if string is a hex number
	int64_t Int64(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint64_t - set bIsHex to true if string is a hex number
	uint64_t UInt64(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

#ifdef DEKAF2_HAS_INT128
	//-----------------------------------------------------------------------------
	/// convert to int128_t - set bIsHex to true if string is a hex number
	int128_t Int128(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to uint128_t - set bIsHex to true if string is a hex number
	uint128_t UInt128(bool bIsHex = false) const noexcept;
	//-----------------------------------------------------------------------------
#endif

	//-----------------------------------------------------------------------------
	/// convert to float
	float Float() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert to double
	double Double() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	bool operator==(KStringView other) const
	//-----------------------------------------------------------------------------
	{
		return size() == other.size() && !compare(other);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	bool operator!=(KStringView other) const
	//-----------------------------------------------------------------------------
	{
		return !operator==(other);
	}

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	bool operator==(KStringViewZ other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_CONSTEXPR_14
	bool operator!=(KStringViewZ other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool operator==(const KString& other) const
	//-----------------------------------------------------------------------------
	{
		return operator==(KStringView(other));
	}

	//-----------------------------------------------------------------------------
	bool operator!=(const KString& other) const
	//-----------------------------------------------------------------------------
	{
		return operator!=(KStringView(other));
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	DEKAF2_ALWAYS_INLINE
	DEKAF2_CONSTEXPR_14
	void unchecked_remove_prefix(size_type n)
	//-----------------------------------------------------------------------------
	{
#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
		m_rep.uncheckedAdvance(n);
#elif defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
		m_rep.unchecked_remove_prefix(n);
#else
		m_rep.remove_prefix(n);
#endif
	}

	//-----------------------------------------------------------------------------
	DEKAF2_ALWAYS_INLINE
	DEKAF2_CONSTEXPR_14
	void unchecked_remove_suffix(size_type n)
	//-----------------------------------------------------------------------------
	{
#if defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW)
		m_rep.uncheckedSubtract(n);
#elif defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
		m_rep.unchecked_remove_suffix(n);
#else
		m_rep.remove_suffix(n);
#endif
	}

	//-----------------------------------------------------------------------------
	void Warn(KStringView sWhere, KStringView sWhat) const;
	//-----------------------------------------------------------------------------

	rep_type m_rep; // NOLINT: clang-tidy wants us to make this private, but it does not know that we need it protected for KStringViewZ

	static constexpr value_type s_0ch = '\0'; // NOLINT: same as above

};

using KStringViewPair = std::pair<KStringView, KStringView>;


//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator==(const char* left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator==(KStringView left, const char* right)
//-----------------------------------------------------------------------------
{
	return left.operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator!=(const char* left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator!=(KStringView left, const char* right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator==(const std::string& left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
inline bool operator==(KStringView left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const std::string& left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
inline bool operator!=(KStringView left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(KStringView(right));
}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator==(sv::string_view left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator==(KStringView left, sv::string_view right)
//-----------------------------------------------------------------------------
{
	return left.operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator!=(sv::string_view left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right.operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool operator!=(KStringView left, sv::string_view right)
//-----------------------------------------------------------------------------
{
	return left.operator!=(KStringView(right));
}
#endif

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17
bool operator<(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return left.compare(right) < 0;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17
bool operator>(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return right < left;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17
bool operator<=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left > right);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_17
bool operator>=(KStringView left, KStringView right)
//-----------------------------------------------------------------------------
{
	return !(left < right);
}


//-----------------------------------------------------------------------------
inline
DEKAF2_CONSTEXPR_14
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
#if defined(DEKAF2_IS_CLANG) || defined(DEKAF2_IS_GCC)
	auto ret = static_cast<const char*>(__builtin_memchr(haystack.data()+pos, needle, haystack.size()-pos));
#else
	auto ret = static_cast<const char*>(memchr(haystack.data()+pos, needle, haystack.size()-pos));
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
	return static_cast<size_t>(found - haystack.data());
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
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return detail::stringview::kFindFirstOfInt(haystack, needle, pos);
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
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return detail::stringview::kFindFirstNotOfInt(haystack, needle, pos);
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
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return detail::stringview::kFindLastOfInt(haystack, needle, pos);
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
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND) \
	|| defined(DEKAF2_USE_FOLLY_STRINGPIECE_AS_KSTRINGVIEW) \
	|| defined(DEKAF2_USE_DEKAF2_STRINGVIEW_AS_KSTRINGVIEW)
	return detail::stringview::kFindLastNotOfInt(haystack, needle, pos);
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
/// Ignore delimiter chars prefixed by odd number of escapes.
size_t kFindUnescaped(KStringView haystack,
                      KStringView::value_type needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// Find delimiter string prefixed by even number of escape characters (0, 2, ...).
/// Ignore delimiter string prefixed by odd number of escapes.
size_t kFindUnescaped(KStringView haystack,
                      KStringView needle,
                      KStringView::value_type chEscape,
                      KStringView::size_type pos = 0);
//-----------------------------------------------------------------------------

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool kStartsWith(KStringView sInput, KStringView sPattern) noexcept
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.size() < sPattern.size()))
	{
		return false;
	}

	if (DEKAF2_UNLIKELY(sPattern.empty()))
	{
		return true;
	}

	return !std::memcmp(sInput.data(), sPattern.data(), sPattern.size());

} // kStartsWith

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool kEndsWith(KStringView sInput, KStringView sPattern) noexcept
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.size() < sPattern.size()))
	{
		return false;
	}

	if (DEKAF2_UNLIKELY(sPattern.empty()))
	{
		return true;
	}

	return !std::memcmp(sInput.data() + sInput.size() - sPattern.size(), sPattern.data(), sPattern.size());

} // kEndsWith

//----------------------------------------------------------------------
inline
bool kContains(KStringView sInput, KStringView sPattern) noexcept
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.size() < sPattern.size()))
	{
		return false;
	}

	if (DEKAF2_UNLIKELY(sPattern.empty()))
	{
		return true;
	}

	return kFind(sInput, sPattern) != KStringView::npos;

} // kContains

//----------------------------------------------------------------------
inline
bool kContains(KStringView sInput, const char ch) noexcept
//----------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(sInput.empty()))
	{
		return false;
	}

	return kFind(sInput, ch) != KStringView::npos;

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
		DEKAF2_CONSTEXPR_14 std::size_t operator()(dekaf2::KStringView s) const noexcept
		{
			return dekaf2::kHash(s.data(), s.size());
		}

		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return dekaf2::kHash(s);
		}
	};

} // namespace std

namespace boost
{
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	/// provide a boost::hash for KStringView
#if (BOOST_VERSION < 106400)
	template<>
	struct hash<dekaf2::KStringView> : public std::unary_function<dekaf2::KStringView, std::size_t>
#else
	template<>
	struct hash<dekaf2::KStringView>
#endif
	//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	{
		DEKAF2_CONSTEXPR_14 std::size_t operator()(dekaf2::KStringView s) const noexcept
		{
			return dekaf2::kHash(s.data(), s.size());
		}

		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return dekaf2::kHash(s);
		}
	};

} // namespace boost

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_14 std::size_t dekaf2::KStringView::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<dekaf2::KStringView>()(*this);
}

#include "bits/kstringviewz.h"
#include "ksplit.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
inline
constexpr
#ifdef _MSC_VER
KStringView::KStringView(const KStringViewZ& svz) noexcept
#else
KStringView::KStringView(KStringViewZ svz) noexcept
#endif
//-----------------------------------------------------------------------------
: KStringView(svz.data(), svz.size())
{
}

//-----------------------------------------------------------------------------
inline
DEKAF2_CONSTEXPR_14
#ifdef _MSC_VER
KStringView& KStringView::operator=(const KStringViewZ& other)
#else
KStringView& KStringView::operator=(KStringViewZ other)
#endif
//-----------------------------------------------------------------------------
{
	assign(other.data(), other.size());
	return *this;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool KStringView::operator==(KStringViewZ other) const
//-----------------------------------------------------------------------------
{
	return size() == other.size() && !compare(KStringView(other));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_14
bool KStringView::operator!=(KStringViewZ other) const
//-----------------------------------------------------------------------------
{
	return !operator==(other);
}

} // end of namespace dekaf2

#include "kstring.h"

namespace dekaf2 {

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
template<typename T, typename...Parms>
T KStringView::Split(Parms&&... parms) const
//-----------------------------------------------------------------------------
{
	T Container;
	kSplit(Container, *this, std::forward<Parms>(parms)...);
	return Container;
}

} // end of namespace dekaf2


