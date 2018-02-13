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
#include "bits/kcppcompat.h"
#include "kformat.h"
#include "kstringview.h"
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
#include <folly/FBString.h>
#endif


namespace dekaf2
{

// forward declaration
class KString;

//----------------------------------------------------------------------
bool kStrIn (const char* sNeedle, const char* sHaystack, char iDelim=',');
//----------------------------------------------------------------------

//----------------------------------------------------------------------
KString kToUpper(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
KString kToLower(KStringView sInput);
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
	typedef folly::fbstring string_type;
#else
	typedef std::string string_type;
#endif

	typedef string_type::traits_type            traits_type;
	typedef string_type::value_type             value_type;
	typedef string_type::allocator_type         allocator_type;
	typedef string_type::size_type              size_type;
	typedef string_type::difference_type        difference_type;
	typedef string_type::reference              reference;
	typedef string_type::const_reference        const_reference;
	typedef string_type::pointer                pointer;
	typedef string_type::const_pointer          const_pointer;
	typedef string_type::iterator               iterator;
	typedef string_type::const_iterator         const_iterator;
	typedef string_type::const_reverse_iterator const_reverse_iterator;
	typedef string_type::reverse_iterator       reverse_iterator;

	static constexpr size_type npos = string_type::npos;

	// Iterators
	inline iterator begin() { return m_rep.begin(); }
	inline const_iterator begin() const { return m_rep.begin(); }
	inline iterator end() { return m_rep.end(); }
	inline const_iterator end() const { return m_rep.end(); }
	inline reverse_iterator rbegin() { return m_rep.rbegin(); }
	inline const_reverse_iterator rbegin() const { return m_rep.rbegin(); }
	inline reverse_iterator rend() { return m_rep.rend(); }
	inline const_reverse_iterator rend() const { return m_rep.rend(); }
	inline const_iterator cbegin() const noexcept { return m_rep.cbegin(); }
	inline const_iterator cend() const noexcept { return m_rep.cend(); }
	inline const_reverse_iterator crbegin() const noexcept { return m_rep.crbegin(); }
	inline const_reverse_iterator crend() const noexcept { return m_rep.crend(); }

	inline size_type size() const { return m_rep.size(); }
	inline size_type length() const { return m_rep.length(); }
	inline size_type max_size() const { return m_rep.max_size(); }
	inline void resize(size_type n, value_type c) { m_rep.resize(n, c); }
	inline void resize(size_type n) { m_rep.resize(n); }
	inline size_type capacity() const { return m_rep.capacity(); }
	inline void reserve(size_type res_arg = 0) { m_rep.reserve(res_arg); }
	inline void clear() { m_rep.clear(); }
	inline bool empty() const { return m_rep.empty(); }
	inline void shrink_to_fit() { m_rep.shrink_to_fit(); }
	
	inline const_reference operator[] (size_type pos) const { return at(pos); }
	inline reference operator[](size_type pos) { return at(pos); }
	inline const_reference at(size_type pos) const { if DEKAF2_UNLIKELY(pos >= size()) { return s_0ch; } return m_rep.at(pos); }
	inline reference at(size_type pos) { if DEKAF2_UNLIKELY(pos >= size()) { *s_0ch_v = '\0'; return *s_0ch_v; } return m_rep.at(pos); }
	inline const_reference back() const { if DEKAF2_UNLIKELY(empty()) { return s_0ch; } return m_rep.back(); }
	inline reference back() { if DEKAF2_UNLIKELY(empty()) { *s_0ch_v = '\0'; return *s_0ch_v; } return m_rep.back(); }
	inline const_reference front() const { if DEKAF2_UNLIKELY(empty()) { return s_0ch; } return m_rep.front(); }
	inline reference front() { if DEKAF2_UNLIKELY(empty()) { return *s_0ch_v; } return m_rep.front(); }

	// Constructors
	KString () = default;
	KString (const KString& str) = default;
	KString (KString&& str) noexcept = default;
	KString (const KString& str, size_type pos, size_type len = npos) : m_rep(str.m_rep, (pos > str.size()) ? str.size() : pos, len){}
	KString (const string_type& sStr) : m_rep(sStr){}
	KString (size_type iCount, value_type ch) : m_rep(iCount, ch){}
	KString (const value_type* s) : m_rep(s?s:""){}
	KString (const value_type* pszString, size_type iCount) : m_rep(pszString?pszString:"", pszString?iCount:0){}
	KString (const value_type* pszString, size_type iRoff, size_type iCount) : m_rep(pszString?pszString:"", pszString?iRoff:0, pszString?iCount:0){}
	template<class _InputIterator>
		KString (_InputIterator first, _InputIterator last) : m_rep(first, last) {}
	KString (string_type&& sStr) noexcept : m_rep(std::move(sStr)){}
	KString (std::initializer_list<value_type> il) : m_rep(il) {}
	KString (KStringView sv) : m_rep(sv.data(), sv.size()) {}
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString (const std::string& sStr) : m_rep(sStr) {}
#endif

	// operator+=
	KString& operator+= (const KString& str){ m_rep += str.m_rep; return *this; }
	KString& operator+= (const string_type& str){ m_rep += str; return *this; }
	KString& operator+= (const value_type ch){ m_rep += ch; return *this; }
	KString& operator+= (const value_type *s){ if (s) m_rep += s; return *this; }
	KString& operator+= (std::initializer_list<value_type> il) { m_rep += il; return *this; }
	KString& operator+= (KStringView sv) { m_rep.append(sv.data(), sv.size()); return *this; }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString& operator+= (const std::string& str){ m_rep += str; return *this; }
#endif

	// operator=
	KString& operator= (const KString& str) = default;
	KString& operator= (KString&& str) noexcept = default;

	// std methods
	KString& append(const KString& str){ m_rep.append(str.m_rep); return *this; }
	KString& append(const KString& str, size_type pos, size_type n = npos) { return append(str.m_rep, pos, n); }
	KString& append(const string_type& str){ m_rep.append(str); return *this; }
	KString& append(const string_type& str, size_type pos, size_type n = npos);
	KString& append(const value_type* str) { m_rep.append(str); return *this; }
	KString& append(const value_type* str, size_type n) { m_rep.append(str, n); return *this; }
	KString& append(size_type n, value_type ch){ m_rep.append(n, ch); return *this; }
	template<class _InputIterator>
		KString& append(_InputIterator first, _InputIterator last) { m_rep.append(first, last); return *this; }
	KString& append(std::initializer_list<value_type> il) { m_rep.append(il); return *this; }
	KString& append(KStringView sv) { m_rep.append(sv.data(), sv.size()); return *this; }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString& append(const std::string& str){ m_rep.append(str); return *this; }
	KString& append(const std::string& str, size_type pos, size_type n = npos);
#endif

	KString& push_back(const value_type chPushBack) { m_rep.push_back(chPushBack); return *this; }
	void pop_back() { m_rep.pop_back(); }

	KString& assign(const KString& str) { m_rep.assign(str.m_rep); return *this; }
	KString& assign(const KString& str, size_type pos, size_type n = npos) { return assign(str.m_rep, pos, n); }
	KString& assign(const string_type& str) { m_rep.assign(str); return *this; }
	KString& assign(const string_type& str, size_type pos, size_type n = npos);
	KString& assign(const value_type* s, size_type n) { m_rep.assign(s, n); return *this; }
	KString& assign(const value_type* str) { m_rep.assign(str); return *this;}
	KString& assign(size_type n, value_type ch) { m_rep.assign(n, ch); return *this;}
	template<class _InputIterator>
		KString& assign(_InputIterator first, _InputIterator last) { m_rep.assign(first, last); return *this; }
	KString& assign(std::initializer_list<value_type> il) { m_rep.assign(il); return *this; }
	KString& assign(KString&& str) { m_rep.assign(std::move(str.m_rep)); return *this; }
	KString& assign(KStringView sv) { m_rep.assign(sv.data(), sv.size()); return *this; }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString& assign(const std::string& str) { m_rep.assign(str); return *this; }
	KString& assign(const std::string& str, size_type pos, size_type n = npos);
#endif

	int compare(const KString& str) const { return KStringView(*this).compare(str); }
	int compare(size_type pos, size_type n, const KString& str) const { return KStringView(*this).compare(pos, n, str); }
	int compare(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) const { return KStringView(*this).compare(pos1, n1, str, pos2, n2); }
	int compare(const string_type& str) const { return KStringView(*this).compare(KStringView(str.data(), str.size())); }
	int compare(size_type pos, size_type n, const string_type& str) const { return KStringView(*this).compare(pos, n, KStringView(str.data(), str.size())); }
	int compare(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos) const { return KStringView(*this).compare(pos1, n1, KStringView(str.data(), str.size()), pos2, n2); }
	int compare(const value_type* s) const { return KStringView(*this).compare(s); }
	int compare(size_type pos, size_type n1, const value_type* s) const { return KStringView(*this).compare(pos, n1, s); }
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2) const { return KStringView(*this).compare(pos, n1, s, n2); }
	int compare(KStringView sv) const { return KStringView(*this).compare(sv); }
	int compare(size_type pos, size_type n1, KStringView sv) const { return KStringView(*this).compare(pos, n1, sv); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	int compare(const std::string& str) const { return KStringView(*this).compare(str); }
	int compare(size_type pos, size_type n, const std::string& str) const { return KStringView(*this).compare(pos, n, str); }
	int compare(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos) const { return KStringView(*this).compare(pos1, n1, str, pos2, n2); }
#endif

	size_type copy(value_type* s, size_type n, size_type pos = 0) const;

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	size_type find(value_type c, size_type pos = 0) const { return kFind(*this, c, pos); }
	size_type find(KStringView sv, size_type pos = 0) const { return kFind(*this, sv, pos); }
	size_type find(const value_type* s, size_type pos, size_type n) const { return find(KStringView(s, n), pos); }
#else
	size_type find(value_type c, size_type pos = 0) const { return m_rep.find(c, pos); }
	size_type find(const value_type* s, size_type pos, size_type n) const { return m_rep.find(s, pos, n); }
	size_type find(KStringView sv, size_type pos = 0) const { return find(sv.data(), pos, sv.size()); }
#endif
	size_type find(const KString& str, size_type pos = 0) const { return find(str.data(), pos, str.size()); }
	size_type find(const string_type& str, size_type pos = 0) const { return find(str.data(), pos, str.size()); }
	size_type find(const value_type* s, size_type pos = 0) const { return find(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find(const std::string& str, size_type pos = 0) const { return find(str.data(), pos, str.size()); }
#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	size_type rfind(value_type c, size_type pos = npos) const { return kRFind(*this, c, pos); }
	size_type rfind(KStringView sv, size_type pos = npos) const { return kRFind(*this, sv, pos); }
	size_type rfind(const value_type* s, size_type pos, size_type n) const { return rfind(KStringView(s, n), pos); }
#else
	size_type rfind(value_type c, size_type pos = npos) const { return m_rep.rfind(c, pos); }
	size_type rfind(const value_type* s, size_type pos, size_type n) const { return m_rep.rfind(s, pos, n); }
	size_type rfind(KStringView sv, size_type pos = npos) const { return rfind(sv.data(), pos, sv.size()); }
#endif
	size_type rfind(const KString& str, size_type pos = npos) const { return rfind(str.data(), pos, str.size()); }
	size_type rfind(const string_type& str, size_type pos = npos) const { return rfind(str.data(), pos, str.size()); }
	size_type rfind(const value_type* s, size_type pos = npos) const { return rfind(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type rfind(const std::string& str, size_type pos = npos) const { return rfind(str.data(), pos, str.size()); }
#endif

	size_type find_first_of(value_type c, size_type pos = 0) const { return find(c, pos); }
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	size_type find_first_of(KStringView sv, size_type pos = 0) const;
	size_type find_first_of(const value_type* s, size_type pos, size_type n) const { return find_first_of(KStringView(s, n), pos); }
#else
	size_type find_first_of(const value_type* s, size_type pos, size_type n) const { return (DEKAF2_UNLIKELY(n == 1)) ? find(*s, pos) : m_rep.find_first_of(s, pos, n); }
	size_type find_first_of(KStringView sv, size_type pos = 0) const { return find_first_of(sv.data(), pos, sv.size()); }
#endif
	size_type find_first_of(const KString& str, size_type pos = 0) const { return find_first_of(str.data(), pos, str.size()); }
	size_type find_first_of(const string_type& str, size_type pos = 0) const { return find_first_of(str.data(), pos, str.size()); }
	size_type find_first_of(const value_type* s, size_type pos = 0) const { return find_first_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_first_of(const std::string& str, size_type pos = 0) const { return find_first_of(str.data(), pos, str.size()); }
#endif

	size_type find_last_of(value_type c, size_type pos = npos) const { return rfind(c, pos); }
#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	size_type find_last_of(KStringView sv, size_type pos = npos) const { return kFindLastOf(*this, sv, pos); }
	size_type find_last_of(const value_type* s, size_type pos, size_type n) const { return find_last_of(KStringView(s, n), pos); }
#else
	size_type find_last_of(const value_type* s, size_type pos, size_type n) const { return (DEKAF2_UNLIKELY(n == 1)) ? rfind(*s, pos) : m_rep.find_last_of(s, pos, n); }
	size_type find_last_of(KStringView sv, size_type pos = npos) const { return find_last_of(sv.data(), pos, sv.size()); }
#endif
	size_type find_last_of(const KString& str, size_type pos = npos) const { return find_last_of(str.data(), pos, str.size()); }
	size_type find_last_of(const string_type& str, size_type pos = npos) const { return find_last_of(str.data(), pos, str.size()); }
	size_type find_last_of(const value_type* s, size_type pos = npos) const { return find_last_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_last_of(const std::string& str, size_type pos = npos) const { return find_last_of(str.data(), pos, str.size()); }
#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	size_type find_first_not_of(value_type c, size_type pos = 0) const { return find_first_not_of(&c, pos, 1); }
	size_type find_first_not_of(KStringView sv, size_type pos = 0) const;
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const { return find_first_not_of(KStringView(s, n), pos); }
#else
	size_type find_first_not_of(value_type c, size_type pos = 0) const { return m_rep.find_first_not_of(c, pos); }
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_first_not_of(s, pos, n); }
	size_type find_first_not_of(KStringView sv, size_type pos = 0) const { return find_first_not_of(sv.data(), pos, sv.size()); }
#endif
	size_type find_first_not_of(const KString& str, size_type pos = 0) const { return find_first_not_of(str.data(), pos, str.size()); }
	size_type find_first_not_of(const string_type& str, size_type pos = 0) const { return find_first_not_of(str.data(), pos, str.size()); }
	size_type find_first_not_of(const value_type* s, size_type pos = 0) const { return find_first_not_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_first_not_of(const std::string& str, size_type pos = 0) const { return find_first_not_of(str.data(), pos, str.size()); }
#endif

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)
	size_type find_last_not_of(value_type c, size_type pos = npos) const { return find_last_not_of(&c, pos, 1); }
	size_type find_last_not_of(KStringView sv, size_type pos = npos) const { return kFindLastNotOf(*this, sv, pos); }
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const { return find_last_not_of(KStringView(s, n), pos); }
#else
	size_type find_last_not_of(value_type c, size_type pos = npos) const { return m_rep.find_last_not_of(c, pos); }
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const { return m_rep.find_last_not_of(s, pos, n); }
	size_type find_last_not_of(KStringView sv, size_type pos = npos) const { return find_last_not_of(sv.data(), pos, sv.size()); }
#endif
	size_type find_last_not_of(const KString& str, size_type pos = npos) const { return find_last_not_of(str.data(), pos, str.size()); }
	size_type find_last_not_of(const string_type& str, size_type pos = npos) const { return find_last_not_of(str.data(), pos, str.size()); }
	size_type find_last_not_of(const value_type* s, size_type pos = npos) const { return find_last_not_of(s, pos, strlen(s)); }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_last_not_of(const std::string& str, size_type pos = npos) const { return find_last_not_of(str.data(), pos, str.size()); }
#endif

	void insert(iterator p, size_type n, value_type c) { m_rep.insert(p, n, c); }
	KString& insert(size_type pos, const KString& str) { return insert(pos, str.m_rep); }
	KString& insert(size_type pos1, const KString& str, size_type pos2, size_type n = npos) { return insert(pos1, str.m_rep, pos2, n); }
	KString& insert(size_type pos, const string_type& str);
	KString& insert(size_type pos1, const string_type& str, size_type pos2, size_type n = npos);
	KString& insert(size_type pos, const value_type* s, size_type n);
	KString& insert(size_type pos, const value_type* s);
	KString& insert(size_type pos, size_type n, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator insert(iterator it, value_type c);
	template<class _InputIterator>
		void insert(const_iterator it, _InputIterator beg, _InputIterator end) { m_rep.insert(it, beg, end); }
	// should be const_iterator with C++11, but is not supported by libstdc++
	KString& insert (iterator it, std::initializer_list<value_type> il);
	KString& insert(size_type pos, KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString& insert(size_type pos, const std::string& str) { return insert(pos, str.data(), str.size()); }
	KString& insert(size_type pos1, const std::string& str, size_type pos2, size_type n = npos);
#endif

	const value_type* c_str() const noexcept { return m_rep.c_str(); }
	const value_type* data() const noexcept { return m_rep.data(); }
	// C++17 supports non-const data(), but gcc does not yet know it..
	value_type* data() noexcept { return &m_rep[0]; }

	KString& erase(size_type pos = 0, size_type n = npos);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator erase(iterator position);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator erase(iterator first, iterator last);

	// borrowed from string_view
	void remove_suffix(size_type n) { if DEKAF2_UNLIKELY(n > size()) { n = size(); } erase(size()-n, n); }
	// borrowed from string_view
	void remove_prefix(size_type n) { erase(0, n); }
	// extension from string_view
	bool remove_suffix(KStringView suffix)
	{
		if DEKAF2_LIKELY(EndsWith(suffix))
		{
			remove_suffix(suffix.size());
			return true;
		}
		return false;
	}
	// extension from string_view
	bool remove_prefix(KStringView prefix)
	{
		if DEKAF2_LIKELY(StartsWith(prefix))
		{
			remove_prefix(prefix.size());
			return true;
		}
		return false;
	}

	KString& replace(size_type pos, size_type n, const KString& str) { return replace(pos, n, str.m_rep); }
	KString& replace(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) { return replace(pos1, n1, str.m_rep, pos2, n2); }
	KString& replace(size_type pos, size_type n, const string_type& str);
	KString& replace(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos);
	KString& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	KString& replace(size_type pos, size_type n1, const value_type* s);
	KString& replace(size_type pos, size_type n1, size_type n2, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const KString& str) { return replace(i1, i2, str.m_rep); }
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const string_type& str);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, const value_type* s, size_type n);
	template<class _InputIterator>
	KString& replace(const_iterator i1, iterator i2, _InputIterator first, _InputIterator last)
	{
		try {
			m_rep.replace(i1, i2, first, last);
		} catch (std::exception& e) {
			log_exception(e, "replace");
		}
		return *this;
	}
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, std::initializer_list<value_type> il);
	KString& replace(size_type pos, size_type n, KStringView sv);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	KString& replace(iterator i1, iterator i2, KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString& replace(size_type pos, size_type n, const std::string& str) { return replace(pos, n, str.data(), str.size()); }
	KString& replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos);
#endif

	KString substr(size_type pos = 0, size_type n = npos) const;

	inline void swap(KString& s) { m_rep.swap(s.m_rep); }

	inline allocator_type get_allocator() const noexcept { return m_rep.get_allocator(); }

	// other methods
	/// print arguments with fmt::format
	template<class... Args>
	KString& Format(Args&&... args)
	{
		m_rep = kFormat(std::forward<Args>(args)...);
		return *this;
	}

	/// print arguments with fmt::printf
	template<class... Args>
	KString& Printf(Args&&... args)
	{
		m_rep = kPrintf(std::forward<Args>(args)...);
		return *this;
	}

	/// replace with regular expression, sReplaceWith may address sub-groups with \\1 etc.
	size_type ReplaceRegex(KStringView sRegEx, KStringView sReplaceWith, bool bReplaceAll = true);

	/// replace one part of the string with another string
	size_type Replace(KStringView sSearch, KStringView sReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace one char of the string with another char
	size_type Replace(value_type chSearch, value_type chReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace any of some chars of the string with another char
	size_type Replace(KStringView sSearch, value_type sReplace, size_type pos = 0, bool bReplaceAll = true);

	/// does the string start with sPattern?
	bool StartsWith(KStringView sPattern) const { return kStartsWith(*this, sPattern); }

	/// does the string end with sPattern?
	bool EndsWith(KStringView sPattern) const { return kEndsWith(*this, sPattern); }

	/// changes the string to lowercase
	KString& MakeLower();

	/// changes the string to uppercase
	KString& MakeUpper();

	/// returns a copy of the string in uppercase
	KString ToUpper() const { return kToUpper(*this); }

	/// returns a copy of the string in lowercase
	KString ToLower() const { return kToLower(*this); }

	/// returns leftmost iCount chars of string
	KStringView Left(size_type iCount) const;

	/// returns substring starting at iStart for iCount chars
	KStringView Mid(size_type iStart, size_type iCount) const { return ToView(iStart, iCount); }

	/// returns rightmost iCount chars of string
	KStringView Right(size_type iCount) const;

	/// pads string at the left up to iWidth size with chPad
	KString& PadLeft(size_t iWidth, value_type chPad = ' ');

	/// pads string at the right up to iWidth size with chPad
	KString& PadRight(size_t iWidth, value_type chPad = ' ');

	/// removes white space from the left of the string
	KString& TrimLeft();
	/// removes chTrim from the left of the string
	KString& TrimLeft(value_type chTrim);
	/// removes any character in sTrim from the left of the string
	KString& TrimLeft(KStringView sTrim);

	/// removes white space from the right of the string
	KString& TrimRight();
	/// removes chTrim from the right of the string
	KString& TrimRight(value_type chTrim);
	/// removes any character in sTrim from the right of the string
	KString& TrimRight(KStringView sTrim);

	/// removes white space from the left and right of the string
	KString& Trim();
	/// removes chTrim from the left and right of the string
	KString& Trim(value_type chTrim);
	/// removes any character in sTrim from the left and right of the string
	KString& Trim(KStringView sTrim);

	/// Clip removing sClipAt and everything to its right if found; otherwise do not alter the string
	bool ClipAt(KStringView sClipAt);

	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(KStringView sClipAtReverse);

	/// remove any occurence of the characters in sIllegalChars
	void RemoveIllegalChars(KStringView sIllegalChars);

	/// convert to representation type
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	inline operator const string_type&() const { return m_rep; }
	inline operator string_type&() { return m_rep; }
#endif

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	/// convert to std::string
	inline std::string ToStdString() const { return m_rep.toStdString(); }
#else
	/// convert to std::string
	inline const std::string& ToStdString() const { return m_rep; }
#endif

	/// return the representation type
	const string_type& str() const { return m_rep; }
	string_type& str() { return m_rep; }

	/// convert to KStringView
	operator KStringView() const { return KStringView(data(), size()); }

	/// return a KStringView
	KStringView ToView() const { return operator KStringView(); }

	/// return a KStringView much like a substr(), but without the cost
	KStringView ToView(size_type pos, size_type n = npos) const;

	/// helper operator to allow KString as formatting arg of fmt::format
	operator fmt::BasicCStringRef<char>() const { return fmt::BasicCStringRef<char>(c_str()); }

	/// is string one of the values in sHaystack, delimited by iDelim?
	bool In (KStringView sHaystack, value_type iDelim=',') const;

#ifdef DEKAF2_WITH_DEPRECATED_KSTRING_MEMBER_FUNCTIONS

	// These functions are either bad in their interfaces, do not
	// belong into the string class, are duplicates of existing
	// functionalities or are badly named. We allow them on request
	// to keep compatibility to the old dekaf KString class.
	// DO NOT USE THEM IN NEW CODE

	/// DEPRECATED - only for compatibility with old code
	bool FindRegex(KStringView regex) const;
	/// DEPRECATED - only for compatibility with old code
	bool FindRegex(KStringView regex, unsigned int* start, unsigned int* end, size_type pos = 0) const;
	/// DEPRECATED - only for compatibility with old code
	size_type SubString(KStringView sReplaceMe, KStringView sReplaceWith, bool bReplaceAll = false) { return Replace(sReplaceMe, sReplaceWith, 0, bReplaceAll); }
	/// DEPRECATED - only for compatibility with old code
	size_type SubRegex(KStringView pszRegEx, KStringView pszReplaceWith, bool bReplaceAll = false, size_type* piIdxOffset = nullptr);
	/// DEPRECATED - only for compatibility with old code
	const value_type* c() const { return c_str(); }
	/// DEPRECATED - only for compatibility with old code
	KString& Append(const value_type* pszAppend) { return append(pszAppend); }

#endif

	// conversions

	//-----------------------------------------------------------------------------
	bool Bool() const noexcept
	//-----------------------------------------------------------------------------
	{
		return (KStringView(*this).Int16() != 0);
	}
	//-----------------------------------------------------------------------------
	int16_t Int16(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).Int16(bIsHex);
	}
	//-----------------------------------------------------------------------------
	uint16_t UInt16(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).UInt16(bIsHex);
	}

	//-----------------------------------------------------------------------------
	int32_t Int32(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).Int32(bIsHex);
	}
	//-----------------------------------------------------------------------------
	uint32_t UInt32(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).UInt32(bIsHex);
	}

	//-----------------------------------------------------------------------------
	int64_t Int64(bool bIsHex = false) const noexcept { return KStringView(*this).Int64(bIsHex); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	uint64_t UInt64(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).UInt64(bIsHex);
	}

	//-----------------------------------------------------------------------------
	int128_t Int128(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).Int128(bIsHex);
	}

	//-----------------------------------------------------------------------------
	uint128_t UInt128(bool bIsHex = false) const noexcept
	//-----------------------------------------------------------------------------
	{
		return KStringView(*this).UInt128(bIsHex);
	}

	//-----------------------------------------------------------------------------
	float Float() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	double Double() const noexcept;
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	template<typename Integer>
	static KString to_string(Integer i)
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_integral<Integer>::value);
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
	static KString to_hexstring(uint64_t i, bool bZeroPad = true, bool bUpperCase = true);
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

	static void log_exception(const std::exception& e, KStringView sWhere);

	static constexpr value_type s_0ch = '\0';
	static value_type s_0ch_v[2];

	string_type m_rep;

}; // KString

//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return left.ToView() == right.ToView();
}

//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.ToView() == KStringView(right.data(), right.size());
}

//-----------------------------------------------------------------------------
inline bool operator==(const std::string& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, KStringView right)
//-----------------------------------------------------------------------------
{
	return left.ToView() == right;
}

//-----------------------------------------------------------------------------
inline bool operator==(KStringView left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
inline bool operator==(const KString& left, const KString::value_type* right)
//-----------------------------------------------------------------------------
{
	return left.ToView() == KStringView(right);
}

//-----------------------------------------------------------------------------
inline bool operator==(const KString::value_type* left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right == left;
}

//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return !(left == right);
}

//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, const std::string& right)
//-----------------------------------------------------------------------------
{
	return left.ToView() != KStringView(right.data(), right.size());
}

//-----------------------------------------------------------------------------
inline bool operator!=(const std::string& left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, KStringView right)
//-----------------------------------------------------------------------------
{
	return left.ToView() != right;
}

//-----------------------------------------------------------------------------
inline bool operator!=(KStringView left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right != left;
}

//-----------------------------------------------------------------------------
inline bool operator!=(const KString& left, const KString::value_type* right)
//-----------------------------------------------------------------------------
{
	return left.ToView() != KStringView(right);
}

//-----------------------------------------------------------------------------
inline bool operator!=(const KString::value_type* left, const KString& right)
//-----------------------------------------------------------------------------
{
	return right != left;
}


//-----------------------------------------------------------------------------
inline std::ostream& operator <<(std::ostream& stream, const KString& str)
//-----------------------------------------------------------------------------
{
	return stream << str.str();
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

} // end of namespace dekaf2

namespace std
{
	std::istream& getline(std::istream& stream, dekaf2::KString& str);
	std::istream& getline(std::istream& stream, dekaf2::KString& str, dekaf2::KString::value_type delimiter);

	/// provide a std::hash for KString
	template<> struct hash<dekaf2::KString>
	{
		typedef dekaf2::KString argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
			return std::hash<dekaf2::KString::string_type>{}(s);
#else
			return std::hash<dekaf2::KString::string_type>{}(s.ToStdString());
#endif
		}
	};

} // end of namespace std

#include <boost/functional/hash.hpp>

namespace boost
{
	/// provide a boost::hash for KString
	template<> struct hash<dekaf2::KString> : public std::unary_function<dekaf2::KString, std::size_t>
	{
		typedef dekaf2::KString argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& s) const noexcept
		{
			// reuse the std::hash, as it knows fbstring already
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
			return std::hash<dekaf2::KString::string_type>{}(s);
#else
			return std::hash<dekaf2::KString::string_type>{}(s.ToStdString());
#endif
		}
	};

} // end of namespace boost

