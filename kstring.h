/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2000-2017, Ridgeware, Inc.
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

/// @file kstring.h
/// dekaf2's own string class - a wrapper around std::string or folly::fbstring
/// that handles most error cases in a benign way and has rapid fast searches

#include <string>
#include <istream>
#include <ostream>
#include <vector>
#include "bits/kcppcompat.h"
#include "bits/kstring_view.h"
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
#include <folly/FBString.h>
#endif
#include <fmt/format.h>

namespace dekaf2
{

// forward declarations
class KString;
class KStringView;
class KStringViewZ;
template<class Value> class KStack;


//----------------------------------------------------------------------
KString kToUpper(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
KString kToLower(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
KString kToUpperLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
KString kToLowerLocale(KStringView sInput);
//----------------------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf2's own string class - a wrapper around std::string
/// that handles most error cases in a benign way and speeds up
/// searching in a spectacular way
class KString 
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	using string_type               = folly::fbstring;
	/// tag to construct a string on an existing malloced buffer
	using AcquireMallocatedString   = folly::AcquireMallocatedString;
	#define DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
#else
	using string_type               = std::string;
	// dummy to allow user code without #ifdefs
	struct AcquireMallocatedString {};
#endif
	using self                      = KString;
	using traits_type               = string_type::traits_type;
	using value_type                = string_type::value_type;
	using allocator_type            = string_type::allocator_type;
	using size_type                 = string_type::size_type;
	using difference_type           = string_type::difference_type;
	using reference                 = string_type::reference;
	using const_reference           = string_type::const_reference;
	using pointer                   = string_type::pointer;
	using const_pointer             = string_type::const_pointer;
	using iterator                  = string_type::iterator;
	using const_iterator            = string_type::const_iterator;
	using const_reverse_iterator    = string_type::const_reverse_iterator;
	using reverse_iterator          = string_type::reverse_iterator;

	static constexpr size_type npos = string_type::npos;

	// Iterators
	iterator begin() { return m_rep.begin(); }
	const_iterator begin() const { return m_rep.begin(); }
	iterator end() { return m_rep.end(); }
	const_iterator end() const { return m_rep.end(); }
	reverse_iterator rbegin() { return m_rep.rbegin(); }
	const_reverse_iterator rbegin() const { return m_rep.rbegin(); }
	reverse_iterator rend() { return m_rep.rend(); }
	const_reverse_iterator rend() const { return m_rep.rend(); }
	const_iterator cbegin() const noexcept { return m_rep.cbegin(); }
	const_iterator cend() const noexcept { return m_rep.cend(); }
	const_reverse_iterator crbegin() const noexcept { return m_rep.crbegin(); }
	const_reverse_iterator crend() const noexcept { return m_rep.crend(); }

	size_type size() const { return m_rep.size(); }
	std::size_t Hash() const;
	size_type length() const { return m_rep.length(); }
	size_type max_size() const { return m_rep.max_size(); }
	void resize(size_type n, value_type c) { m_rep.resize(n, c); }
	void resize(size_type n) { m_rep.resize(n); }
	size_type capacity() const { return m_rep.capacity(); }
	void reserve(size_type res_arg = 0) { m_rep.reserve(res_arg); }
	void clear() { m_rep.clear(); }
	bool empty() const { return m_rep.empty(); }
	void shrink_to_fit() { m_rep.shrink_to_fit(); }
	void resize_uninitialized(size_type n);

	const_reference operator[] (size_type pos) const { return at(pos); }
	reference operator[](size_type pos) { return at(pos); }
	const_reference at(size_type pos) const { if DEKAF2_UNLIKELY(pos >= size()) { return s_0ch; } return m_rep.at(pos); }
	reference at(size_type pos) { if DEKAF2_UNLIKELY(pos >= size()) { *s_0ch_v = '\0'; return *s_0ch_v; } return m_rep.at(pos); }
	const_reference back() const { if DEKAF2_UNLIKELY(empty()) { return s_0ch; } return m_rep.back(); }
	reference back() { if DEKAF2_UNLIKELY(empty()) { *s_0ch_v = '\0'; return *s_0ch_v; } return m_rep.back(); }
	const_reference front() const { if DEKAF2_UNLIKELY(empty()) { return s_0ch; } return m_rep.front(); }
	reference front() { if DEKAF2_UNLIKELY(empty()) { return *s_0ch_v; } return m_rep.front(); }

	// Constructors
	KString () = default;
	KString (const KString& str) = default;
	KString (KString&& str) noexcept = default;
	KString (const KString& str, size_type pos, size_type len = npos) : m_rep(str.m_rep, (pos > str.size()) ? str.size() : pos, len) {}
	KString (KStringView sv);
	KString (KStringViewZ svz);
	KString (const string_type& sStr) : m_rep(sStr) {}
	KString (string_type&& sStr) : m_rep(std::move(sStr)) {}
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString (const std::string& sStr) : m_rep(sStr) {}
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	KString (const sv::string_view& str) : m_rep(str.data(), str.size()) {}
#endif
	KString (const value_type* s) : m_rep(s?s:"") {}

	KString (size_type iCount, value_type ch) : m_rep(iCount, ch) {}
	KString (const value_type* pszString, size_type iCount) : m_rep(pszString?pszString:"", pszString?iCount:0) {}
	KString (const value_type* pszString, size_type iRoff, size_type iCount) : m_rep(pszString?pszString:"", pszString?iRoff:0, pszString?iCount:0) {}
	template<class _InputIterator>
		KString (_InputIterator first, _InputIterator last) : m_rep(first, last) {}
	KString (std::initializer_list<value_type> il) : m_rep(il) {}
#ifdef DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
	// nonstandard constructor to move an existing malloced buffer into the string
	KString (value_type *s, size_type n, size_type c, AcquireMallocatedString a)
	: m_rep(s, n, c, a) {}
#else
	// no buffer move possible, simply copy and release the input buffer
	KString (value_type *s, size_type n, size_type c, AcquireMallocatedString a)
	: KString (s, n) { if (s) delete(s); }
#endif

	self& operator= (const KString& str) = default;
	self& operator= (KString&& str) noexcept = default;
	self& operator= (const value_type* str) { return assign(str); }
	self& operator= (value_type ch) { assign(1, ch); return *this; }
	self& operator= (std::initializer_list<value_type> il) { return assign(il); }

	// operator+=
	self& operator+= (const KString& str) { m_rep += str.m_rep; return *this; }
	self& operator+= (const string_type& str) { m_rep += str; return *this; }
	self& operator+= (const value_type ch) { m_rep += ch; return *this; }
	self& operator+= (const value_type *s) { if (s) m_rep += s; return *this; }
	self& operator+= (std::initializer_list<value_type> il) { m_rep += il; return *this; }
	self& operator+= (KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& operator+= (const std::string& str) { m_rep += str; return *this; }
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	self& operator+= (std::string_view str) { return append(str); }
#endif

	// std methods
	self& append(const KString& str) { m_rep.append(str.m_rep); return *this; }
	self& append(const KString& str, size_type pos, size_type n = npos) { return append(str.m_rep, pos, n); }
	self& append(const string_type& str) { m_rep.append(str); return *this; }
	self& append(const string_type& str, size_type pos, size_type n = npos);
	self& append(const value_type* str) { if (str) m_rep.append(str); return *this; }
	self& append(const value_type* str, size_type n) { if (str) m_rep.append(str, n); return *this; }
	self& append(size_type n, value_type ch) { m_rep.append(n, ch); return *this; }
	template<class _InputIterator>
		self& append(_InputIterator first, _InputIterator last) { m_rep.append(first, last); return *this; }
	self& append(std::initializer_list<value_type> il) { m_rep.append(il); return *this; }
	self& append(KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& append(const std::string& str) { m_rep.append(str); return *this; }
	self& append(const std::string& str, size_type pos, size_type n = npos);
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	self& append(std::string_view str) { return append(str.data(), str.size()); }
#endif

	self& push_back(const value_type chPushBack) { m_rep.push_back(chPushBack); return *this; }
	void pop_back() { m_rep.pop_back(); }

	self& assign(const KString& str) { m_rep.assign(str.m_rep); return *this; }
	self& assign(const KString& str, size_type pos, size_type n = npos) { return assign(str.m_rep, pos, n); }
	self& assign(const string_type& str) { m_rep.assign(str); return *this; }
	self& assign(const string_type& str, size_type pos, size_type n = npos);
	self& assign(string_type&& str) { m_rep.assign(std::move(str)); return *this; }
	self& assign(const value_type* s, size_type n) { if (s) m_rep.assign(s, n); else m_rep.clear(); return *this; }
	self& assign(const value_type* str) { if (str) m_rep.assign(str); else m_rep.clear(); return *this;}
	self& assign(size_type n, value_type ch) { m_rep.assign(n, ch); return *this;}
	template<class _InputIterator>
		self& assign(_InputIterator first, _InputIterator last) { m_rep.assign(first, last); return *this; }
	self& assign(std::initializer_list<value_type> il) { m_rep.assign(il); return *this; }
	self& assign(KString&& str) { m_rep.assign(std::move(str.m_rep)); return *this; }
	self& assign(KStringView sv);
	self& assign(KStringViewZ sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& assign(const std::string& str) { m_rep.assign(str); return *this; }
	self& assign(const std::string& str, size_type pos, size_type n = npos);
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	self& assign(std::string_view sv) { return assign(sv.data(), sv.size()); }
#endif

	int compare(const KString& str) const;
	int compare(size_type pos, size_type n, const KString& str) const;
	int compare(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) const;
	int compare(const string_type& str) const;
	int compare(size_type pos, size_type n, const string_type& str) const;
	int compare(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos) const;
	int compare(const value_type* s) const;
	int compare(size_type pos, size_type n1, const value_type* s) const;
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2) const;
	int compare(KStringView sv) const;
	int compare(size_type pos, size_type n1, KStringView sv) const;
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	int compare(const std::string& str) const;
	int compare(size_type pos, size_type n, const std::string& str) const;
	int compare(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos) const;
#endif

	size_type copy(value_type* s, size_type n, size_type pos = 0) const;

	size_type find(value_type c, size_type pos = 0) const;
	size_type find(KStringView sv, size_type pos = 0) const;
	size_type find(const value_type* s, size_type pos, size_type n) const;
	size_type find(const KString& str, size_type pos = 0) const { return find(str.data(), pos, str.size()); }
	size_type find(const string_type& str, size_type pos = 0) const { return find(str.data(), pos, str.size()); }
	size_type find(const value_type* s, size_type pos = 0) const { return find(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find(const std::string& str, size_type pos = 0) const { return find(str.data(), pos, str.size()); }
#endif

	size_type rfind(value_type c, size_type pos = npos) const;
	size_type rfind(KStringView sv, size_type pos = npos) const;
	size_type rfind(const value_type* s, size_type pos, size_type n) const;
	size_type rfind(const KString& str, size_type pos = npos) const { return rfind(str.data(), pos, str.size()); }
	size_type rfind(const string_type& str, size_type pos = npos) const { return rfind(str.data(), pos, str.size()); }
	size_type rfind(const value_type* s, size_type pos = npos) const { return rfind(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type rfind(const std::string& str, size_type pos = npos) const { return rfind(str.data(), pos, str.size()); }
#endif

	size_type find_first_of(value_type c, size_type pos = 0) const { return find(c, pos); }
	size_type find_first_of(KStringView sv, size_type pos = 0) const;
	size_type find_first_of(const value_type* s, size_type pos, size_type n) const;
	size_type find_first_of(const KString& str, size_type pos = 0) const { return find_first_of(str.data(), pos, str.size()); }
	size_type find_first_of(const string_type& str, size_type pos = 0) const { return find_first_of(str.data(), pos, str.size()); }
	size_type find_first_of(const value_type* s, size_type pos = 0) const { return find_first_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_first_of(const std::string& str, size_type pos = 0) const { return find_first_of(str.data(), pos, str.size()); }
#endif

	size_type find_last_of(value_type c, size_type pos = npos) const { return rfind(c, pos); }
	size_type find_last_of(KStringView sv, size_type pos = npos) const;
	size_type find_last_of(const value_type* s, size_type pos, size_type n) const;
	size_type find_last_of(const KString& str, size_type pos = npos) const { return find_last_of(str.data(), pos, str.size()); }
	size_type find_last_of(const string_type& str, size_type pos = npos) const { return find_last_of(str.data(), pos, str.size()); }
	size_type find_last_of(const value_type* s, size_type pos = npos) const { return find_last_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_last_of(const std::string& str, size_type pos = npos) const { return find_last_of(str.data(), pos, str.size()); }
#endif

	size_type find_first_not_of(value_type c, size_type pos = 0) const;
	size_type find_first_not_of(KStringView sv, size_type pos = 0) const;
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const;
	size_type find_first_not_of(const KString& str, size_type pos = 0) const { return find_first_not_of(str.data(), pos, str.size()); }
	size_type find_first_not_of(const string_type& str, size_type pos = 0) const { return find_first_not_of(str.data(), pos, str.size()); }
	size_type find_first_not_of(const value_type* s, size_type pos = 0) const { return find_first_not_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_first_not_of(const std::string& str, size_type pos = 0) const { return find_first_not_of(str.data(), pos, str.size()); }
#endif

	size_type find_last_not_of(value_type c, size_type pos = npos) const;
	size_type find_last_not_of(KStringView sv, size_type pos = npos) const;
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const;
	size_type find_last_not_of(const KString& str, size_type pos = npos) const { return find_last_not_of(str.data(), pos, str.size()); }
	size_type find_last_not_of(const string_type& str, size_type pos = npos) const { return find_last_not_of(str.data(), pos, str.size()); }
	size_type find_last_not_of(const value_type* s, size_type pos = npos) const { return find_last_not_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_last_not_of(const std::string& str, size_type pos = npos) const { return find_last_not_of(str.data(), pos, str.size()); }
#endif

	void insert(iterator p, size_type n, value_type c) { m_rep.insert(p, n, c); }
	self& insert(size_type pos, const KString& str) { return insert(pos, str.m_rep); }
	self& insert(size_type pos1, const KString& str, size_type pos2, size_type n = npos) { return insert(pos1, str.m_rep, pos2, n); }
	self& insert(size_type pos, const string_type& str);
	self& insert(size_type pos1, const string_type& str, size_type pos2, size_type n = npos);
	self& insert(size_type pos, const value_type* s, size_type n);
	self& insert(size_type pos, const value_type* s);
	self& insert(size_type pos, size_type n, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator insert(iterator it, value_type c);
	template<class _InputIterator>
		void insert(const_iterator it, _InputIterator beg, _InputIterator end) { m_rep.insert(it, beg, end); }
	// should be const_iterator with C++11, but is not supported by libstdc++
	self& insert (iterator it, std::initializer_list<value_type> il);
	self& insert(size_type pos, KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& insert(size_type pos, const std::string& str) { return insert(pos, str.data(), str.size()); }
	self& insert(size_type pos1, const std::string& str, size_type pos2, size_type n = npos);
#endif

	const value_type* c_str() const noexcept { return m_rep.c_str(); }
	const value_type* data() const noexcept { return m_rep.data(); }
	// C++17 supports non-const data(), but gcc does not yet know it..
	value_type* data() noexcept { return &m_rep[0]; }

	self& erase(size_type pos = 0, size_type n = npos);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator erase(iterator position);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator erase(iterator first, iterator last);

	// borrowed from string_view
	void remove_suffix(size_type n);
	// borrowed from string_view
	void remove_prefix(size_type n);
	// extension from string_view
	bool remove_suffix(KStringView suffix);
	// extension from string_view
	bool remove_prefix(KStringView prefix);

	self& replace(size_type pos, size_type n, const KString& str) { return replace(pos, n, str.m_rep); }
	self& replace(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) { return replace(pos1, n1, str.m_rep, pos2, n2); }
	self& replace(size_type pos, size_type n, const string_type& str);
	self& replace(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos);
	self& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	self& replace(size_type pos, size_type n1, const value_type* s);
	self& replace(size_type pos, size_type n1, size_type n2, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	self& replace(iterator i1, iterator i2, const KString& str) { return replace(i1, i2, str.m_rep); }
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	self& replace(iterator i1, iterator i2, const string_type& str);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	self& replace(iterator i1, iterator i2, const value_type* s, size_type n);
	template<class _InputIterator>
	self& replace(const_iterator i1, iterator i2, _InputIterator first, _InputIterator last)
	{
#ifdef DEKAF2_EXCEPTIONS
		m_rep.replace(i1, i2, first, last);
#else
		DEKAF2_TRY {
			m_rep.replace(i1, i2, first, last);
		} DEKAF2_CATCH (std::exception& e) {
			log_exception(e, "replace");
		}
		return *this;
#endif
	}
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	self& replace(iterator i1, iterator i2, std::initializer_list<value_type> il);
	self& replace(size_type pos, size_type n, KStringView sv);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	self& replace(iterator i1, iterator i2, KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& replace(size_type pos, size_type n, const std::string& str) { return replace(pos, n, str.data(), str.size()); }
	self& replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos);
#endif

	/// substring starting at zero-based position "pos" for "n" chars.  if "n" is not specified return the rest of the string starting at "pos"
	KString substr(size_type pos = 0, size_type n = npos) const;

	void swap(KString& s) { m_rep.swap(s.m_rep); }

	allocator_type get_allocator() const noexcept { return m_rep.get_allocator(); }

	/// print arguments with fmt::format
	template<class... Args>
	self& Format(Args&&... args);

	/// print arguments with fmt::printf
	template<class... Args>
	self& Printf(Args&&... args);

	/// match with regular expression and return the overall match (group 0)
	KStringView MatchRegex(KStringView sRegEx, size_type pos = 0) const;

	/// match with regular expression and return all match groups
	std::vector<KStringView> MatchRegexGroups(KStringView sRegEx, size_type pos = 0) const;

	/// replace with regular expression, sReplaceWith may address sub-groups with \\1 etc.
	size_type ReplaceRegex(KStringView sRegEx, KStringView sReplaceWith, bool bReplaceAll = true);

	/// replace one part of the string with another string
	size_type Replace(KStringView sSearch, KStringView sReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace one char of the string with another char
	size_type Replace(value_type chSearch, value_type chReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace any of some chars of the string with another char
	size_type Replace(KStringView sSearch, value_type sReplace, size_type pos = 0, bool bReplaceAll = true);

	// std::C++20
	/// does the string start with sPattern?
	bool starts_with(KStringView sPattern) const noexcept;

	// std::C++20
	/// does the string start with sChar?
	bool starts_with(value_type	ch) const noexcept;

	// std::C++20
	/// does the string end with sPattern?
	bool ends_with(KStringView sPattern) const noexcept;

	// std::C++20
	/// does the string end with sChar?
	bool ends_with(value_type ch) const noexcept;

	/// does the string start with sPattern? (Now deprecated, replace by starts_with())
	bool StartsWith(KStringView sPattern) const noexcept;

	/// does the string end with sPattern? (Now deprecated, replace by ends_with())
	bool EndsWith(KStringView sPattern) const noexcept;

	/// does the string contain the sPattern?
	bool Contains(KStringView sPattern) const;

	/// does the string contain the ch?
	bool Contains(const value_type ch) const;

	/// changes the string to lowercase (UTF8)
	self& MakeLower();

	/// changes the string to uppercase (UTF8)
	self& MakeUpper();

	/// changes the string to lowercase according to the current locale
	self& MakeLowerLocale();

	/// changes the string to uppercase according to the current locale
	self& MakeUpperLocale();

	/// returns a copy of the string in uppercase (UTF8)
	KString ToUpper() const;

	/// returns a copy of the string in lowercase (UTF8)
	KString ToLower() const;

	/// returns a copy of the string in uppercase according to the current locale
	KString ToUpperLocale() const;

	/// returns a copy of the string in lowercase according to the current locale
	KString ToLowerLocale() const;

	/// returns leftmost iCount chars of string
	KStringView Left(size_type iCount) const;

	/// returns substring starting at iStart for iCount chars
	KStringView Mid(size_type iStart, size_type iCount) const;

	/// returns rightmost iCount chars of string
	KStringViewZ Right(size_type iCount) const;

	/// pads string at the left up to iWidth size with chPad
	self& PadLeft(size_t iWidth, value_type chPad = ' ');

	/// pads string at the right up to iWidth size with chPad
	self& PadRight(size_t iWidth, value_type chPad = ' ');

	/// removes white space from the left of the string
	self& TrimLeft();
	/// removes chTrim from the left of the string
	self& TrimLeft(value_type chTrim);
	/// removes any character in sTrim from the left of the string
	self& TrimLeft(KStringView sTrim);

	/// removes white space from the right of the string
	self& TrimRight();
	/// removes chTrim from the right of the string
	self& TrimRight(value_type chTrim);
	/// removes any character in sTrim from the right of the string
	self& TrimRight(KStringView sTrim);

	/// removes white space from the left and right of the string
	self& Trim();
	/// removes chTrim from the left and right of the string
	self& Trim(value_type chTrim);
	/// removes any character in sTrim from the left and right of the string
	self& Trim(KStringView sTrim);

	/// Collapses multiple white space to one space
	self& Collapse();
	/// Collapses consecutive chars in svCollapse to one instance of chTo
	self& Collapse(KStringView svCollapse, value_type chTo);

	/// Collapses multiple white space to one space and trims left and right white space
	self& CollapseAndTrim();
	/// Collapses consecutive chars in svCollapse to one instance of chTo and trims the same chars left and right
	self& CollapseAndTrim(KStringView svCollapse, value_type chTo);

	/// Clip removing sClipAt and everything to its right if found; otherwise do not alter the string
	bool ClipAt(KStringView sClipAt);

	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(KStringView sClipAtReverse);

	/// remove any occurence of the characters in sIllegalChars
	void RemoveIllegalChars(KStringView sIllegalChars);

	/// convert to representation type
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	operator const string_type&() const { return m_rep; }
	operator string_type&() { return m_rep; }
#endif

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	/// convert to std::string
	std::string ToStdString() const { return m_rep.toStdString(); }
#else
	/// convert to std::string
	const std::string& ToStdString() const { return m_rep; }
#endif

	/// return the representation type
	const string_type& str() const { return m_rep; }

	string_type& str() { return m_rep; }

	/// return a KStringViewZ
	KStringViewZ ToView() const;

	/// return a KStringViewZ much like a substr(), but without the cost
	KStringViewZ ToView(size_type pos) const;

	/// return a KStringView much like a substr(), but without the cost
	KStringView ToView(size_type pos, size_type n) const;

	/// helper operator to allow KString as formatting arg of fmt::format
	operator fmt::string_view() const;

	/// is string one of the values in sHaystack, delimited by iDelim?
	bool In (KStringView sHaystack, value_type iDelim=',') const;

#ifdef DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS

	// These functions are either bad in their interfaces, do not
	// belong into the string class, are duplicates of existing
	// functionalities or are badly named. We allow them on request
	// to keep compatibility to the old dekaf KString class.
	// DO NOT USE THEM IN NEW CODE

	/// DEPRECATED - only for compatibility with old code
	DEKAF2_DEPRECATED("only for compatibility with old code")
	bool FindRegex(KStringView regex) const;
	/// DEPRECATED - only for compatibility with old code
	DEKAF2_DEPRECATED("only for compatibility with old code")
	bool FindRegex(KStringView regex, unsigned int* start, unsigned int* end, size_type pos = 0) const;
	/// DEPRECATED - only for compatibility with old code
	DEKAF2_DEPRECATED("only for compatibility with old code")
	size_type SubString(KStringView sReplaceMe, KStringView sReplaceWith, bool bReplaceAll = false) { return Replace(sReplaceMe, sReplaceWith, 0, bReplaceAll); }
	/// DEPRECATED - only for compatibility with old code
	DEKAF2_DEPRECATED("only for compatibility with old code")
	size_type SubRegex(KStringView pszRegEx, KStringView pszReplaceWith, bool bReplaceAll = false, size_type* piIdxOffset = nullptr);
	/// DEPRECATED - only for compatibility with old code
	DEKAF2_DEPRECATED("only for compatibility with old code")
	const value_type* c() const { return c_str(); }
	/// DEPRECATED - only for compatibility with old code
	DEKAF2_DEPRECATED("only for compatibility with old code")
	self& Append(const value_type* pszAppend) { return append(pszAppend); }

#endif

	// conversions

	/// convert to bool
	bool Bool() const noexcept;
	/// convert to int16_t - set bIsHex to true if string is a hex number
	int16_t Int16(bool bIsHex = false) const noexcept;
	/// convert to uint16_t - set bIsHex to true if string is a hex number
	uint16_t UInt16(bool bIsHex = false) const noexcept;
	/// convert to int32_t - set bIsHex to true if string is a hex number
	int32_t Int32(bool bIsHex = false) const noexcept;
	/// convert to uint32_t - set bIsHex to true if string is a hex number
	uint32_t UInt32(bool bIsHex = false) const noexcept;
	/// convert to int64_t - set bIsHex to true if string is a hex number
	int64_t Int64(bool bIsHex = false) const noexcept;
	/// convert to uint64_t - set bIsHex to true if string is a hex number
	uint64_t UInt64(bool bIsHex = false) const noexcept;
#ifdef DEKAF2_HAS_INT128
	/// convert to int128_t - set bIsHex to true if string is a hex number
	int128_t Int128(bool bIsHex = false) const noexcept;
	/// convert to uint128_t - set bIsHex to true if string is a hex number
	uint128_t UInt128(bool bIsHex = false) const noexcept;
#endif
	/// convert to float
	float Float() const noexcept;
	/// convert to double
	double Double() const noexcept;

	//-----------------------------------------------------------------------------
	/// convert any integer to KString
	template<typename Integer>
	static KString to_string(Integer i)
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_integral<Integer>::value, "need integral type");
		if (std::is_signed<Integer>::value)
		{
			return signed_to_string(static_cast<int64_t>(i));
		}
		else
		{
			return unsigned_to_string(static_cast<uint64_t>(i));
		}
	}

	//-----------------------------------------------------------------------------
	static KString to_string(float f)
	//-----------------------------------------------------------------------------
	{
		return std::to_string(f);
	}

	//-----------------------------------------------------------------------------
	static KString to_string(double f)
	//-----------------------------------------------------------------------------
	{
		return std::to_string(f);
	}

	//-----------------------------------------------------------------------------
	/// convert integer into a hex string
	static KString to_hexstring(uint64_t i, bool bZeroPad = true, bool bUpperCase = true);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool operator==(const KString& other) const
	//-----------------------------------------------------------------------------
	{
		return m_rep == other.m_rep;
	}

	//-----------------------------------------------------------------------------
	bool operator!=(const KString& other) const
	//-----------------------------------------------------------------------------
	{
		return m_rep != other.m_rep;
	}

	//-----------------------------------------------------------------------------
	bool operator==(KStringView other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool operator!=(KStringView other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool operator==(KStringViewZ other) const;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	bool operator!=(KStringViewZ other) const;
	//-----------------------------------------------------------------------------

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	static KString signed_to_string(int64_t i);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static KString unsigned_to_string(uint64_t i);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void log_exception(const std::exception& e, const char* sWhere);
	//-----------------------------------------------------------------------------

	static constexpr value_type s_0ch = '\0';
	static value_type s_0ch_v[2];

	string_type m_rep;

}; // KString

} // end of namespace dekaf2


// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// KString is now complete - here follow KString inline methods that need
// KStringView and / or KStringViewZ being complete as well

#include "kstringview.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
inline KString::KString(KStringView sv)
//-----------------------------------------------------------------------------
    : m_rep(sv.data(), sv.size())
{}

//-----------------------------------------------------------------------------
inline KString::KString(KStringViewZ sv)
//-----------------------------------------------------------------------------
	: m_rep(sv.data(), sv.size())
{}

//-----------------------------------------------------------------------------
inline KString& KString::operator+= (KStringView sv)
//-----------------------------------------------------------------------------
{
	m_rep.append(sv.data(), sv.size());
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::assign(KStringView sv)
//-----------------------------------------------------------------------------
{
	m_rep.assign(sv.data(), sv.size());
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::assign(KStringViewZ svz)
//-----------------------------------------------------------------------------
{
	m_rep.assign(svz.data(), svz.size());
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::append(KStringView sv)
//-----------------------------------------------------------------------------
{
	m_rep.append(sv.data(), sv.size());
	return *this;
}

//-----------------------------------------------------------------------------
inline int KString::compare(const KString& str) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(str);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n, const KString& str) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n, str.ToView());
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos1, n1, str.ToView(), pos2, n2);
}

//-----------------------------------------------------------------------------
inline int KString::compare(const string_type& str) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(KStringView(str.data(), str.size()));
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n, const string_type& str) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n, KStringView(str.data(), str.size()));
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos1, n1, KStringView(str.data(), str.size()), pos2, n2);
}

//-----------------------------------------------------------------------------
inline int KString::compare(const value_type* s) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(s);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n1, const value_type* s) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, s);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n1, const value_type* s, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, s, n2);
}

//-----------------------------------------------------------------------------
inline int KString::compare(KStringView sv) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(sv);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n1, KStringView sv) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, sv);
}

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING

//-----------------------------------------------------------------------------
inline int KString::compare(const std::string& str) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(str);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n, const std::string& str) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n, str);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos1, n1, str, pos2, n2);
}

#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFind(*this, c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFind(*this, sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find(c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(sv.data(), pos, sv.size());
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return m_rep.find(s, pos, n);
}

#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kRFind(*this, c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kRFind(*this, sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return rfind(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.rfind(c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(sv.data(), pos, sv.size());
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return m_rep.rfind(s, pos, n);
}

#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return ToView().find_first_of(sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_first_of(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_of(sv.data(), pos, sv.size());
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(n == 1))
	{
		return find(*s, pos);
	}
	else
	{
		return m_rep.find_first_of(s, pos, n);
	}
}

#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindLastOf(*this, sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_last_of(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_of(sv.data(), pos, sv.size());

}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	if (DEKAF2_UNLIKELY(n == 1))
	{
		return rfind(*s, pos);
	}
	else
	{
		return m_rep.find_last_of(s, pos, n);
	}
}

#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(&c, pos, 1);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(KStringView(s, n), pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return ToView().find_first_not_of(sv, pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_first_not_of(c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_first_not_of(s, pos, n);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(sv.data(), pos, sv.size());
}

#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(&c, pos, 1);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindLastNotOf(*this, sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_last_not_of(c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_last_not_of(s, pos, n);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(sv.data(), pos, sv.size());
}

#endif

//-----------------------------------------------------------------------------
inline void KString::remove_suffix(size_type n)
//-----------------------------------------------------------------------------
{
	if DEKAF2_UNLIKELY(n > size())
	{
		n = size();
	}
	erase(size()-n, n);
}

//-----------------------------------------------------------------------------
inline void KString::remove_prefix(size_type n)
//-----------------------------------------------------------------------------
{
	erase(0, n);
}

//-----------------------------------------------------------------------------
inline bool KString::remove_suffix(KStringView suffix)
//-----------------------------------------------------------------------------
{
	if DEKAF2_LIKELY(ends_with(suffix))
	{
		remove_suffix(suffix.size());
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
inline bool KString::remove_prefix(KStringView prefix)
//-----------------------------------------------------------------------------
{
	if DEKAF2_LIKELY(starts_with(prefix))
	{
		remove_prefix(prefix.size());
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
inline bool KString::starts_with(KStringView sPattern) const noexcept
//-----------------------------------------------------------------------------
{
	return kStartsWith(*this, sPattern);
}

//-----------------------------------------------------------------------------
inline bool KString::ends_with(KStringView sPattern) const noexcept
//-----------------------------------------------------------------------------
{
	return kEndsWith(*this, sPattern);
}

//-----------------------------------------------------------------------------
inline bool KString::starts_with(value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kStartsWith(*this, KStringView(std::addressof(ch), 1));
}

//-----------------------------------------------------------------------------
inline bool KString::ends_with(value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kEndsWith(*this, KStringView(std::addressof(ch), 1));
}

//-----------------------------------------------------------------------------
inline bool KString::StartsWith(KStringView sPattern) const noexcept
//-----------------------------------------------------------------------------
{
	return starts_with(sPattern);
}

//-----------------------------------------------------------------------------
inline bool KString::EndsWith(KStringView sPattern) const noexcept
//-----------------------------------------------------------------------------
{
	return ends_with(sPattern);
}

//-----------------------------------------------------------------------------
inline bool KString::Contains(KStringView sPattern) const
//-----------------------------------------------------------------------------
{
	return kContains(*this, sPattern);
}

//-----------------------------------------------------------------------------
inline bool KString::Contains(const KString::value_type ch) const
//-----------------------------------------------------------------------------
{
	return kContains(*this, ch);
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpper() const
//-----------------------------------------------------------------------------
{
	return kToUpper(*this);
}

//-----------------------------------------------------------------------------
inline KString KString::ToLower() const
//-----------------------------------------------------------------------------
{
	return kToLower(*this);
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpperLocale() const
//-----------------------------------------------------------------------------
{
	return kToUpperLocale(*this);
}

//-----------------------------------------------------------------------------
inline KString KString::ToLowerLocale() const
//-----------------------------------------------------------------------------
{
	return kToLowerLocale(*this);
}

//-----------------------------------------------------------------------------
inline KStringView KString::Mid(size_type iStart, size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView(iStart, iCount);
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::ToView() const
//-----------------------------------------------------------------------------
{
	return KStringViewZ(*this);
}

//-----------------------------------------------------------------------------
inline bool KString::In(KStringView sHaystack, value_type iDelim) const
//-----------------------------------------------------------------------------
{
	return ToView().In(sHaystack, iDelim);
}

//-----------------------------------------------------------------------------
inline bool KString::Bool() const noexcept
//-----------------------------------------------------------------------------
{
	return (ToView().Bool());
}

//-----------------------------------------------------------------------------
inline int16_t KString::Int16(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().Int16(bIsHex);
}

//-----------------------------------------------------------------------------
inline uint16_t KString::UInt16(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().UInt16(bIsHex);
}

//-----------------------------------------------------------------------------
inline int32_t KString::Int32(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().Int32(bIsHex);
}

//-----------------------------------------------------------------------------
inline uint32_t KString::UInt32(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().UInt32(bIsHex);
}

//-----------------------------------------------------------------------------
inline int64_t KString::Int64(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	 return ToView().Int64(bIsHex);
}
	
//-----------------------------------------------------------------------------
inline uint64_t KString::UInt64(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().UInt64(bIsHex);
}

#ifdef DEKAF2_HAS_INT128
//-----------------------------------------------------------------------------
inline int128_t KString::Int128(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().Int128(bIsHex);
}

//-----------------------------------------------------------------------------
inline uint128_t KString::UInt128(bool bIsHex) const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().UInt128(bIsHex);
}
#endif

//-----------------------------------------------------------------------------
inline KStringView KString::MatchRegex(KStringView sRegEx, size_type pos) const
//-----------------------------------------------------------------------------
{
	return ToView().MatchRegex(sRegEx, pos);
}

//-----------------------------------------------------------------------------
inline std::vector<KStringView> KString::MatchRegexGroups(KStringView sRegEx, size_type pos) const
//-----------------------------------------------------------------------------
{
	return ToView().MatchRegexGroups(sRegEx, pos);
}

// KString inline methods until here
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++




//-----------------------------------------------------------------------------
inline bool KString::operator==(KStringView other) const
//-----------------------------------------------------------------------------
{
	return other.operator==(*this);
}

//-----------------------------------------------------------------------------
inline bool KString::operator!=(KStringView other) const
//-----------------------------------------------------------------------------
{
	return other.operator!=(*this);
}

//-----------------------------------------------------------------------------
inline bool KString::operator==(KStringViewZ other) const
//-----------------------------------------------------------------------------
{
	return other.operator==(*this);
}

//-----------------------------------------------------------------------------
inline bool KString::operator!=(KStringViewZ other) const
//-----------------------------------------------------------------------------
{
	return other.operator!=(*this);
}

//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator==(const std::string& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right.ToView().operator==(KStringView(left));
}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, const std::string_view& right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator==(const std::string_view& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right.ToView().operator==(KStringView(left));
}
#endif

//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, const char* right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator==(const char* left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right.ToView().operator==(KStringView(left));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator!=(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const std::string& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right.ToView().operator!=(KStringView(left));
}

#ifdef DEKAF2_HAS_STD_STRING_VIEW
//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, const std::string_view& right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator!=(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const std::string_view& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right.ToView().operator!=(KStringView(left));
}
#endif

//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, const char* right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator!=(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const char* left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right.ToView().operator!=(KStringView(left));
}

//-----------------------------------------------------------------------------
inline std::istream& operator >>(std::istream& stream, KString& str)
//-----------------------------------------------------------------------------
{
	return stream >> str.str();
}

//-----------------------------------------------------------------------------
inline KString operator+(const KStringView& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	KString temp;
	temp.reserve(left.size() + right.size());
	temp += left;
	temp += right;
	return temp;
}

//-----------------------------------------------------------------------------
inline KString operator+(const KString::value_type* left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	KStringView sv(left);
	return sv + right;
}

//-----------------------------------------------------------------------------
inline KString operator+(const KStringView& left, KString::value_type right)
//-----------------------------------------------------------------------------
{
	KString temp(left);
	temp.reserve(left.size() + 1);
	temp += right;
	return temp;
}

//-----------------------------------------------------------------------------
inline KString operator+(KString::value_type left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	KString temp;
	temp.reserve(right.size() + 1);
	temp += left;
	temp += right;
	return temp;
}

// now all operator+() with rvalues (only make sense for the rvalue as the left param)

//-----------------------------------------------------------------------------
inline KString operator+(KString&& left, const KStringView& right)
//-----------------------------------------------------------------------------
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

//-----------------------------------------------------------------------------
inline KString operator+(KString&& left, KString::value_type right)
//-----------------------------------------------------------------------------
{
	KString temp(std::move(left));
	temp += right;
	return temp;
}

//------------------------------------------------------------------------------
inline std::size_t kReplace(KString& string,
                            KStringView sSearch,
                            KStringView sReplaceWith,
                            KString::size_type pos = 0,
                            bool bReplaceAll = true)
//------------------------------------------------------------------------------
{
	return string.Replace(sSearch, sReplaceWith, pos, bReplaceAll);
}

inline namespace literals {

	/// provide a string literal for KString
	inline dekaf2::KString operator"" _ks(const char *data, std::size_t size)
	{
		return {data, size};
	}

} // namespace literals

} // end of namespace dekaf2

#include "kformat.h"

namespace dekaf2 {

//----------------------------------------------------------------------
/// helper operator to allow KString as formatting arg of fmt::format
inline KString::operator fmt::string_view() const
//----------------------------------------------------------------------
{
	return fmt::string_view(data(), size());
}

//----------------------------------------------------------------------
template<class... Args>
KString& KString::Format(Args&&... args)
//----------------------------------------------------------------------
{
	*this = kFormat(std::forward<Args>(args)...);
	return *this;
}

//----------------------------------------------------------------------
/// print arguments with fmt::printf
template<class... Args>
KString& KString::Printf(Args&&... args)
//----------------------------------------------------------------------
{
	*this = kPrintf(std::forward<Args>(args)...);
	return *this;
}

} // end of namespace dekaf2

namespace std
{
	std::istream& getline(std::istream& stream, dekaf2::KString& str);
	std::istream& getline(std::istream& stream, dekaf2::KString& str, dekaf2::KString::value_type delimiter);

	/// provide a std::hash for KString
	template<> struct hash<dekaf2::KString>
	{
		// we actually use a KStringView as the parameter, as this avoids
		// accidentially constructing a KString if coming from a KStringView
		// or char* in a template that uses KString as elements
		constexpr std::size_t operator()(dekaf2::KStringView sv) const noexcept
		{
			return dekaf2::hash_bytes_FNV(sv.data(), sv.size());
		}
	};

	// make sure comparisons work without construction of KString
	template<> struct equal_to<dekaf2::KString>
	{
		using is_transparent = void;

		bool operator()(dekaf2::KStringView s1, dekaf2::KStringView s2) const
		{
			return s1 == s2;
		}
	};

	// make sure comparisons work without construction of KString
	template<> struct less<dekaf2::KString>
	{
		using is_transparent = void;

		bool operator()(dekaf2::KStringView s1, dekaf2::KStringView s2) const
		{
			return s1 < s2;
		}
	};

} // end of namespace std

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for KString
	template<> struct hash<dekaf2::KString> : public std::unary_function<dekaf2::KString, std::size_t>
	{
		// we actually use a KStringView as the parameter, as this avoids
		// accidentially constructing a KString if coming from a KStringView
		// or char* in a template that uses KString as elements
		constexpr std::size_t operator()(dekaf2::KStringView s) const noexcept
		{
			return dekaf2::hash_bytes_FNV(s.data(), s.size());
		}
};

} // end of namespace boost

//----------------------------------------------------------------------
inline std::size_t dekaf2::KString::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<dekaf2::KString>()(*this);
}

