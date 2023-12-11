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
/// that offers most string functions from languages like Python or Javascript,
/// handles most error cases in a benign way and speeds up searching
/// up to 50 times compared to std::string implementations

#include "kcompatibility.h"
#include "bits/kstring_view.h"
#include "ktemplate.h"
#include "kctype.h"
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
#include <folly/FBString.h>
#endif
#include <fmt/format.h>
#include <string>
#include <istream>
#include <ostream>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

// forward declarations
class KString;
class KStringView;
class KStringViewZ;

#if defined(DEKAF2_USE_FBSTRING_AS_KSTRING) || \
	(defined(DEKAF2_IS_APPLE_CLANG) && DEKAF2_CLANG_VERSION < 120000)
/// a string type used for string& pars in parameter lists (output string parameters)
using KStringRef = KString;
#else
/// a string type used for string& pars in parameter lists (output string parameters)
/// - converts to and from KString without effort
using KStringRef = std::string;
#endif

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase (UTF8)
DEKAF2_PUBLIC
KString kToUpper(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase (UTF8)
DEKAF2_PUBLIC
KString kToLower(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
DEKAF2_PUBLIC
KString kToUpperLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
DEKAF2_PUBLIC
KString kToLowerLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase assuming ASCII encoding
DEKAF2_PUBLIC
KString kToUpperASCII(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase assuming ASCII encoding
DEKAF2_PUBLIC
KString kToLowerASCII(KStringView sInput);
//----------------------------------------------------------------------

namespace detail {

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	using KStringStringType = folly::fbstring;
#else
	using KStringStringType = std::string;
#endif

template<class T>
struct is_kstring_move_assignable
: std::integral_constant<
	bool,
	std::is_same<KString, T>::value ||
	std::is_same<KStringStringType, T>::value
> {};

} // end of namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf2's own string class - a wrapper around std::string or folly::fbstring
/// that offers most string functions from languages like Python or Javascript,
/// handles most error cases in a benign way and speeds up searching
/// up to 50 times compared to std::string implementations
#ifndef DEKAF2_IS_WINDOWS
class DEKAF2_PUBLIC DEKAF2_GSL_OWNER() KString
#else
class DEKAF2_PUBLIC KString
#endif
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using string_type = detail::KStringStringType;
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	/// tag to construct a string on an existing malloced buffer
	using AcquireMallocatedString   = folly::AcquireMallocatedString;
	#define DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
#else
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

	// standard constructors
	KString ()                                                = default;
	KString (const KString& str)                              = default;
	KString (KString&& str) noexcept                          = default;

	// converting constructors
	KString (const value_type* s)                             : m_rep(s?s:"") {}
	KString (const std::string& s)                            : m_rep(s.data(), s.size()) {}
#ifndef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString (std::string&& s) noexcept                        : m_rep(std::move(s)) {}
#else
	KString (const folly::fbstring& s)                        : m_rep(s) {}
	KString (folly::fbstring&& s)                             : m_rep(std::move(s)) {}
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	KString (const std::string_view& s)                       : m_rep(s.data(), s.size()) {}
#endif
	KString (const KStringView& sv);
	KString (const KStringViewZ& svz);
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	explicit KString (const T& sv);

	// other constructors
	KString (const KString& str, size_type pos, size_type n = npos) : m_rep(str.m_rep, (pos > str.size()) ? str.size() : pos, n) {}
	KString (size_type n, value_type ch)                      : m_rep(n, ch) {}
	KString (const value_type* s, size_type n)                : m_rep(s ? s : "", s ? n : 0) {}
	template<class _InputIterator>
	KString (_InputIterator first, _InputIterator last)       : m_rep(first, last) {}
	KString (std::initializer_list<value_type> il)            : m_rep(il) {}
#ifdef DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
	// nonstandard constructor to move an existing malloced buffer into the string
	KString (value_type *s, size_type n, size_type c, AcquireMallocatedString a) : m_rep(s, n, c, a) {}
#else
	// no buffer move possible, simply copy and release the input buffer
	KString (value_type *s, size_type n, size_type c, AcquireMallocatedString a) : KString (s, n) { if (s) free(s); }
#endif
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	KString (const T& sv, size_type pos, size_type n);

	// assignment operators
	self& operator= (KString str) noexcept                    { swap(str); return *this;     }
	self& operator= (value_type ch);
	self& operator= (const value_type *s);
	template<typename T,
	    typename std::enable_if<
			detail::is_kstringview_assignable<T>::value &&
			!std::is_same<T, std::string>::value
		, int>::type = 0
	>
	self& operator= (const T& sv);
	self& operator= (std::nullptr_t) = delete; // C++23..

	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	self& operator+= (const T& sv);
	self& operator+= (const value_type ch)                    { push_back(ch); return *this; }
	self& operator+= (std::initializer_list<value_type> il);

	iterator                begin()                  noexcept { return m_rep.begin();        }
	const_iterator          begin()            const noexcept { return m_rep.begin();        }
	iterator                end()                    noexcept { return m_rep.end();          }
	const_iterator          end()              const noexcept { return m_rep.end();          }
	reverse_iterator        rbegin()                 noexcept { return m_rep.rbegin();       }
	const_reverse_iterator  rbegin()           const noexcept { return m_rep.rbegin();       }
	reverse_iterator        rend()                   noexcept { return m_rep.rend();         }
	const_reverse_iterator  rend()             const noexcept { return m_rep.rend();         }
	const_iterator          cbegin()           const noexcept { return m_rep.cbegin();       }
	const_iterator          cend()             const noexcept { return m_rep.cend();         }
	const_reverse_iterator  crbegin()          const noexcept { return m_rep.crbegin();      }
	const_reverse_iterator  crend()            const noexcept { return m_rep.crend();        }

	const value_type*       c_str()            const noexcept { return m_rep.c_str();        }
	const value_type*       data()             const noexcept { return m_rep.data();         }
	// C++17 supports non-const data(), but gcc does not yet know it..
	value_type*             data()                   noexcept { return &m_rep[0];            }

	size_type               size()             const          { return m_rep.size();         }
	size_type               length()           const          { return m_rep.length();       }
	size_type               max_size()         const          { return m_rep.max_size();     }
	void                    resize(size_type n, value_type c) { m_rep.resize(n, c);          }
	void                    resize(size_type n)               { m_rep.resize(n);             }
	size_type               capacity()         const          { return m_rep.capacity();     }
	void                    reserve(size_type res_arg = 0)    { m_rep.reserve(res_arg);      }
	void                    clear()                           { m_rep.clear();               }
	bool                    empty()            const          { return m_rep.empty();        }
	void                    shrink_to_fit()                   { m_rep.shrink_to_fit();       }
	void                    resize_uninitialized(size_type n);
	std::size_t             Hash()             const;
	std::size_t             CaseHash()         const;

	const_reference operator[](size_type pos)  const          { return at(pos);              }
	reference       operator[](size_type pos)                 { return at(pos);              }
	const_reference         at(size_type pos)  const          { if DEKAF2_UNLIKELY(pos >= size()) {                 return s_0ch;   } return m_rep[pos];    }
	reference               at(size_type pos)                 { if DEKAF2_UNLIKELY(pos >= size()) { s_0ch_v = '\0'; return s_0ch_v; } return m_rep[pos];    }
	const_reference         back()             const          { if DEKAF2_UNLIKELY(empty())       {                 return s_0ch;   } return m_rep.back();  }
	reference               back()                            { if DEKAF2_UNLIKELY(empty())       { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.back();  }
	const_reference         front()            const          { if DEKAF2_UNLIKELY(empty())       {                 return s_0ch;   } return m_rep.front(); }
	reference               front()                           { if DEKAF2_UNLIKELY(empty())       { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.front(); }

	self& append(const value_type* str)                       { m_rep.append(str ? str : "");       return *this; }
	self& append(const value_type* str, size_type n)          { m_rep.append(str ? str : "", n);    return *this; }
	self& append(size_type n, value_type ch)                  { m_rep.append(n, ch);                return *this; }
	template<class _InputIterator>
	self& append(_InputIterator first, _InputIterator last)   { m_rep.append(first, last);          return *this; }
	self& append(std::initializer_list<value_type> il)        { m_rep.append(il);                   return *this; }
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	self& append(const T& sv);
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	self& append(const T& sv, size_type pos, size_type n = npos);

	void  push_back(const value_type chPushBack)              { m_rep.push_back(chPushBack);                      }
	void  pop_back()                                          { if DEKAF2_LIKELY(!empty()) { m_rep.pop_back(); }  }

	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	self& assign(const T& sv);
	self& assign(const KString&& str)                         { m_rep.assign(std::move(str.m_rep)); return *this; }
	self& assign(size_type n, value_type ch)                  { m_rep.assign(n, ch);                return *this; }
	template<class _InputIterator>
	self& assign(_InputIterator first, _InputIterator last)   { m_rep.assign(first, last);          return *this; }
	self& assign(std::initializer_list<value_type> il)        { m_rep.assign(il);                   return *this; }
	self& assign(KString&& str) noexcept                      { m_rep.assign(std::move(str.m_rep)); return *this; }
	self& assign(const value_type* s, size_type n)            { m_rep.assign(s ? s : "", n);        return *this; }
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	self& assign(const T& sv, size_type pos, size_type n = npos);

	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	int compare(const T& sv)                                                       const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	int compare(size_type pos, size_type n1, const T& sv)                          const;
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2)    const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	int compare(size_type pos, size_type n1, const T& sv, size_type pos2, size_type n2 = npos) const;

	size_type copy(value_type* s, size_type n, size_type pos = 0)                  const;

	size_type find(value_type c, size_type pos = 0)                                const;
	size_type find(KStringView sv, size_type pos = 0)                              const;
	size_type find(const value_type* s, size_type pos = 0)                         const;
	size_type find(const value_type* s, size_type pos, size_type n)                const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	size_type find(const T& sv, size_type pos = 0)                                 const;

	size_type rfind(value_type c, size_type pos = npos)                            const;
	size_type rfind(KStringView sv, size_type pos = npos)                          const;
	size_type rfind(const value_type* s, size_type pos = npos)                     const;
	size_type rfind(const value_type* s, size_type pos, size_type n)               const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	size_type rfind(const T& sv, size_type pos = npos)                             const;

	size_type find_first_of(value_type c, size_type pos = 0)                       const;
	size_type find_first_of(KStringView sv, size_type pos = 0)                     const;
	size_type find_first_of(const value_type* s, size_type pos = 0)                const;
	size_type find_first_of(const value_type* s, size_type pos, size_type n)       const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	size_type find_first_of(const T& sv, size_type pos = 0)                        const;

	size_type find_last_of(value_type c, size_type pos = npos)                     const;
	size_type find_last_of(KStringView sv, size_type pos = npos)                   const;
	size_type find_last_of(const value_type* s, size_type pos = npos)              const;
	size_type find_last_of(const value_type* s, size_type pos, size_type n)        const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	size_type find_last_of(const T& sv, size_type pos = npos)                      const;

	size_type find_first_not_of(value_type c, size_type pos = 0)                   const { return find_first_not_of(&c, pos, 1);                  }
	size_type find_first_not_of(KStringView sv, size_type pos = 0)                 const;
	size_type find_first_not_of(const value_type* s, size_type pos = 0)            const;
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n)   const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	size_type find_first_not_of(const T& sv, size_type pos = 0)                    const;

	size_type find_last_not_of(value_type c, size_type pos = npos)                 const { return find_last_not_of(&c, pos, 1);                   }
	size_type find_last_not_of(KStringView sv, size_type pos = npos)               const;
	size_type find_last_not_of(const value_type* s, size_type pos = npos)          const;
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n)    const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	size_type find_last_not_of(const T& sv, size_type pos = npos)                  const;

	void  insert(iterator p, size_type n, value_type c)                                  { m_rep.insert(p, n, c);                                 }
	self& insert(size_type pos, KStringView sv);
	self& insert(size_type pos1, KStringView sv, size_type pos2, size_type n = npos);
	self& insert(size_type pos, const value_type* s, size_type n);
	self& insert(size_type pos, size_type n, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator insert(iterator it, value_type c);
	template<class _InputIterator>
	void insert(const_iterator it, _InputIterator beg, _InputIterator end)               { m_rep.insert(it, beg, end);                            }
	// should be const_iterator with C++11, but is not supported by libstdc++
	iterator insert (iterator it, std::initializer_list<value_type> il);
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	self& insert(size_type pos, const T& sv);
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	self& insert(size_type pos1, const T& sv, size_type pos2, size_type n = npos);

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
	// extension from string_view
	template<typename T, typename std::enable_if<std::is_same<T, std::remove_cv<value_type>::type>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_14
	bool remove_prefix(T ch)
	{
		if (DEKAF2_LIKELY(starts_with(ch)))
		{
			m_rep.erase(0, 1);
			return true;
		}
		return false;
	}
	// extension from string_view
	template<typename T, typename std::enable_if<std::is_same<T, std::remove_cv<value_type>::type>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_14
	bool remove_suffix(T ch)
	{
		if (DEKAF2_LIKELY(ends_with(ch)))
		{
			m_rep.erase(size()-1, 1);
			return true;
		}
		return false;
	}

	self& replace(size_type pos, size_type n, KStringView sv);
	self& replace(size_type pos1, size_type n1, KStringView sv, size_type pos2, size_type n2);
	self& replace(iterator i1, iterator i2, KStringView sv);
	self& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	self& replace(size_type pos, size_type n1, size_type n2, value_type c);
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
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	self& replace(size_type pos, size_type n, T sv);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	self& replace(iterator i1, iterator i2, T sv);
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	self& replace(size_type pos1, size_type n1, T sv, size_type pos2, size_type n2);

	/// substring starting at zero-based position "pos" for "n" chars.  if "n" is not specified return the rest of the string starting at "pos"
	KString substr(size_type pos = 0, size_type n = npos) const &
	{
		return DEKAF2_UNLIKELY(pos > size())
		  ? self()
		  : self(data() + pos, std::min(n, size() - pos));
	}

	/// substring starting at zero-based position "pos" for "n" chars.  if "n" is not specified return the rest of the string starting at "pos"
	KString substr(size_type pos = 0, size_type n = npos) &&;

	void swap(KString& other)     { using std::swap; swap(m_rep, other.m_rep); }
#ifndef DEKAF2_USE_FBSTRING_AS_KSTRING
	void swap(std::string& other) { using std::swap; swap(m_rep, other);       }
#endif

	allocator_type get_allocator() const noexcept { return m_rep.get_allocator(); }

	/// print arguments with fmt::format
	template<class... Args>
	self& Format(Args&&... args) &;

	/// print arguments with fmt::format
	template<class... Args>
	self&& Format(Args&&... args) && { return std::move(Format(std::forward<Args>(args)...)); }

	/// match with regular expression and return the overall match (group 0)
	KStringView MatchRegex(const KStringView sRegEx, size_type pos = 0) const;

	/// match with regular expression and return all match groups
	std::vector<KStringView> MatchRegexGroups(const KStringView sRegEx, size_type pos = 0) const;

	/// replace with regular expression, sReplaceWith may address sub-groups with \\1 etc., modifies string and returns number of replacements made
	size_type ReplaceRegex(const KStringView sRegEx, const KStringView sReplaceWith, bool bReplaceAll = true);

	/// replace part of the string with another string, modifies string and returns number of replacements made
	size_type Replace(const KStringView sSearch, const KStringView sReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace char of the string with another char, modifies string and returns number of replacements made
	size_type Replace(value_type chSearch, value_type chReplace, size_type pos = 0, bool bReplaceAll = true);

	/// find string with caseless ASCII compare
	size_type FindCaselessASCII(const KStringView sSearch, size_type pos = 0) const noexcept;

	/// does the string contain sSubString (caseless ASCII compare)
	bool ContainsCaselessASCII(const KStringView sSubString) const noexcept;

	// std::C++20
	/// does the string start with sSubString?
	bool starts_with(KStringView sSubString) const noexcept;

	// std::C++20
	/// does the string start with sChar?
	bool starts_with(value_type	ch) const noexcept;

	// std::C++20
	/// does the string end with sSubString?
	bool ends_with(KStringView sSubString) const noexcept;

	// std::C++20
	/// does the string end with sChar?
	bool ends_with(value_type ch) const noexcept;

	// std::C++23
	/// does the string contain the ch?
	bool contains(value_type ch) const noexcept;

	// std::C++23
	/// does the string contain the sSubString?
	bool contains(KStringView sSubString) const noexcept;

	/// does the string start with sSubString? (Now deprecated, replace by starts_with())
	bool StartsWith(KStringView sSubString) const noexcept;

	/// does the string end with sSubString? (Now deprecated, replace by ends_with())
	bool EndsWith(KStringView sSubString) const noexcept;

	/// does the string contain the sSubString? (Now deprecated, replace by contains())
	bool Contains(KStringView sSubString) const noexcept;

	/// does the string contain the ch? (Now deprecated, replace by contains())
	bool Contains(value_type ch) const noexcept;

	/// changes the string to lowercase (UTF8)
	self& MakeLower() &;

	/// changes the string to lowercase (UTF8)
	self&& MakeLower() && { return std::move(MakeLower()); }

	/// changes the string to uppercase (UTF8)
	self& MakeUpper() &;

	/// changes the string to uppercase (UTF8)
	self&& MakeUpper() && { return std::move(MakeUpper()); }

	/// changes the string to lowercase according to the current locale (does not work with UTF8 strings)
	self& MakeLowerLocale() &;

	/// changes the string to lowercase according to the current locale (does not work with UTF8 strings)
	self&& MakeLowerLocale() && { return std::move(MakeLowerLocale()); }

	/// changes the string to uppercase according to the current locale (does not work with UTF8 strings)
	self& MakeUpperLocale() &;

	/// changes the string to uppercase according to the current locale (does not work with UTF8 strings)
	self&& MakeUpperLocale() && { return std::move(MakeUpperLocale()); }

	/// changes the string to lowercase assuming ASCII encoding
	self& MakeLowerASCII() &;

	/// changes the string to lowercase assuming ASCII encoding
	self&& MakeLowerASCII() && { return std::move(MakeLowerASCII()); }

	/// changes the string to uppercase assuming ASCII encoding
	self& MakeUpperASCII() &;

	/// changes the string to uppercase assuming ASCII encoding
	self&& MakeUpperASCII() && { return std::move(MakeUpperASCII()); }

	/// returns a copy of the string in uppercase (UTF8)
	KString ToUpper() const;

	/// returns a copy of the string in lowercase (UTF8)
	KString ToLower() const;

	/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
	KString ToUpperLocale() const &;

	/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
	self&& ToUpperLocale() && { return std::move(MakeUpperLocale()); }

	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	KString ToLowerLocale() const &;

	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	self&& ToLowerLocale() && { return std::move(MakeLowerLocale()); }

	/// returns a copy of the string in uppercase assuming ASCII encoding
	KString ToUpperASCII() const &;

	/// returns a copy of the string in uppercase assuming ASCII encoding
	self&& ToUpperASCII() && { return std::move(MakeUpperASCII()); }

	/// returns a copy of the string in lowercase assuming ASCII encoding
	KString ToLowerASCII() const &;

	/// returns a copy of the string in lowercase assuming ASCII encoding
	self&& ToLowerASCII() && { return std::move(MakeLowerASCII()); }

	/// returns leftmost iCount chars of string
	KStringView Left(size_type iCount) const;

	/// returns substring starting at iStart until end of string
	KStringViewZ Mid(size_type iStart) const;

	/// returns substring starting at iStart for iCount chars
	KStringView Mid(size_type iStart, size_type iCount) const;

	/// returns rightmost iCount chars of string
	KStringViewZ Right(size_type iCount) const;

	/// returns leftmost iCount codepoints of string
	KStringView LeftUTF8(size_type iCount) const;

	/// returns substring starting at codepoint iStart until end of string
	KStringViewZ MidUTF8(size_type iStart) const;

	/// returns substring starting at codepoint iStart for iCount codepoints
	KStringView MidUTF8(size_type iStart, size_type iCount) const;

	/// returns rightmost codepoints chars of string
	KStringViewZ RightUTF8(size_type iCount) const;

	/// returns KCcodePoint at UTF8 position iCount
	KCodePoint AtUTF8(size_type iCount) const;

	/// returns true if string contains UTF8 runs
	bool HasUTF8() const;

	/// returns the count of unicode codepoints (or, UTF8 sequences)
	size_type SizeUTF8() const;

	/// pads string at the left up to iWidth size with chPad
	self& PadLeft(size_t iWidth, value_type chPad = ' ') &;
	/// pads string at the left up to iWidth size with chPad
	self&& PadLeft(size_t iWidth, value_type chPad = ' ') && { return std::move(PadLeft(iWidth, chPad)); }

	/// pads string at the right up to iWidth size with chPad
	self& PadRight(size_t iWidth, value_type chPad = ' ') &;
	/// pads string at the right up to iWidth size with chPad
	self&& PadRight(size_t iWidth, value_type chPad = ' ') && { return std::move(PadRight(iWidth, chPad)); }

	/// removes white space from the left of the string
	self& TrimLeft() &;
	/// removes white space from the left of the string
	self&& TrimLeft() && { return std::move(TrimLeft()); }
	/// removes chTrim from the left of the string
	self& TrimLeft(value_type chTrim) &;
	/// removes chTrim from the left of the string
	self&& TrimLeft(value_type chTrim) && { return std::move(TrimLeft(chTrim)); }
	/// removes any character in sTrim from the left of the string
	self& TrimLeft(KStringView sTrim) &;
	/// removes any character in sTrim from the left of the string
	self&& TrimLeft(KStringView sTrim) &&;

	/// removes white space from the right of the string
	self& TrimRight() &;
	/// removes white space from the right of the string
	self&& TrimRight() && { return std::move(TrimRight()); }
	/// removes chTrim from the right of the string
	self& TrimRight(value_type chTrim) &;
	/// removes chTrim from the right of the string
	self&& TrimRight(value_type chTrim) && { return std::move(TrimRight(chTrim)); }
	/// removes any character in sTrim from the right of the string
	self& TrimRight(KStringView sTrim)&;
	/// removes any character in sTrim from the right of the string
	self&& TrimRight(KStringView sTrim) &&;

	/// removes white space from the left and right of the string
	self& Trim() &;
	/// removes white space from the left and right of the string
	self&& Trim() && { return std::move(Trim()); }
	/// removes chTrim from the left and right of the string
	self& Trim(value_type chTrim) &;
	/// removes chTrim from the left and right of the string
	self&& Trim(value_type chTrim) && { return std::move(Trim(chTrim)); }
	/// removes any character in sTrim from the left and right of the string
	self& Trim(KStringView sTrim) &;
	/// removes any character in sTrim from the left and right of the string
	self&& Trim(KStringView sTrim) &&;

	/// Collapses multiple white space to one space
	self& Collapse() &;
	/// Collapses multiple white space to one space
	self&& Collapse() && { return std::move(Collapse()); }
	/// Collapses consecutive chars in svCollapse to one instance of chTo
	self& Collapse(KStringView svCollapse, value_type chTo) &;
	/// Collapses consecutive chars in svCollapse to one instance of chTo
	self&& Collapse(KStringView svCollapse, value_type chTo) &&;

	/// Collapses multiple white space to one space and trims left and right white space
	self& CollapseAndTrim() &;
	/// Collapses multiple white space to one space and trims left and right white space
	self&& CollapseAndTrim() && { return std::move(CollapseAndTrim()); }
	/// Collapses consecutive chars in svCollapse to one instance of chTo and trims the same chars left and right
	self& CollapseAndTrim(KStringView svCollapse, value_type chTo) &;
	/// Collapses consecutive chars in svCollapse to one instance of chTo and trims the same chars left and right
	self&& CollapseAndTrim(KStringView svCollapse, value_type chTo) &&;

	/// Clip removing sClipAt and everything to its right if found; otherwise do not alter the string
	bool ClipAt(KStringView sClipAt);

	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(KStringView sClipAtReverse);

	/// Remove any occurence of the characters in sChars, returns count of removed chars
	size_type RemoveChars(KStringView sChars);

	/// Deprecated alias for RemoveChars, please replace by RemoveChars()
	size_type RemoveIllegalChars(KStringView sIllegalChars);

	/// Splits string into token container using delimiters, trim, and escape. Returned
	/// Container is a sequence, like a vector, or an associative container like a map.
	/// @return a new Container. Default is a std::vector<KStringView>.
	/// @param svDelim a string view of delimiter characters. Defaults to ",".
	/// @param svPairDelim exists only for associative containers: a string view that is used to separate keys and values in the sequence. Defaults to "=".
	/// @param svTrim a string containing chars to remove from token ends. Defaults to " \f\n\r\t\v\b".
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

	/// Append a sequence of tokens. If the container is an associative type (e.g. a map),
	/// key-value pairs are added.
	/// @return *this
	/// @param svDelim a stringview that is inserted between elements
	/// @param svPairDelim a stringview that is inserted between key and value of an
	/// associative element
	template<typename T, typename... Parms>
	self& Join(const T& Container, Parms&&... parms) &;

	/// Append a sequence of tokens. If the container is an associative type (e.g. a map),
	/// key-value pairs are added.
	/// @return *this
	/// @param svDelim a stringview that is inserted between elements
	/// @param svPairDelim a stringview that is inserted between key and value of an
	/// associative element
	template<typename T, typename... Parms>
	self&& Join(const T& Container, Parms&&... parms) && { return std::move(Join(Container, std::forward<Parms>(parms)...)); }

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	/// convert to std::string
	std::string ToStdString()        const { return m_rep.toStdString(); }
#else
	/// convert to std::string
	const std::string& ToStdString() const & { return m_rep;             }
	/// convert to std::string
	std::string& ToStdString() &           { return m_rep;               }
	/// convert to std::string
	std::string&& ToStdString() &&         { return std::move(m_rep);    }
#endif

	/// return the string type
	const string_type& str()       const & { return m_rep;               }
	/// return the string type non-const
	string_type& str() &                   { return m_rep;               }
	/// return the string type as an rvalue
	string_type&& str() &&                 { return std::move(m_rep);    }

	/// return a KStringViewZ
	KStringViewZ ToView() const;

	/// return a KStringViewZ much like a substr(), but without the cost
	KStringViewZ ToView(size_type pos) const;

	/// return a KStringView much like a substr(), but without the cost
	KStringView ToView(size_type pos, size_type n) const;

	/// test if the string is non-empty
	explicit operator bool()      const    { return !empty();            }

	/// return the string type
	operator const string_type&() const &  { return m_rep;               }
	/// return the string type non-const
	operator string_type&() &              { return m_rep;               }
	/// return the string type as an rvalue
	operator string_type&&() &&            { return std::move(m_rep);    }

	/// helper operator to allow KString as formatting arg of fmt::format
	operator fmt::string_view() const;

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	/// allowing conversion into std::string_view
	operator std::string_view()   const { return std::string_view(m_rep.data(), m_rep.size()); }
#endif
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
	/// print arguments with fmt::printf - now DEPRECATED, please use Format()!
	template<class... Args>
	DEKAF2_DEPRECATED("only for compatibility with old code")
	self& Printf(Args&&... args);

#endif

	// conversions

	/// returns bool representation of the string:
	/// "true" --> true
	/// "false" --> false
	/// as well as non-0 --> true
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
	static KString to_string(Integer i, uint16_t iBase = 10, bool bZeroPad = false, bool bUppercase = true)
	//-----------------------------------------------------------------------------
	{
		static_assert(std::is_integral<Integer>::value, "need integral type");
		if (std::is_signed<Integer>::value)
		{
			return signed_to_string(static_cast<int64_t>(i), iBase, bZeroPad, bUppercase);
		}
		return unsigned_to_string(static_cast<uint64_t>(i), iBase, bZeroPad, bUppercase);
	}

	//-----------------------------------------------------------------------------
	/// convert a float into a string
	static KString to_string(float f);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert a double into a string
	static KString to_string(double f);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert integer into a hex string
	static KString to_hexstring(uint64_t i, bool bZeroPad = true, bool bUpperCase = true)
	//-----------------------------------------------------------------------------
	{
		return unsigned_to_string(i, 16, bZeroPad, bUpperCase);
	}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	static KString signed_to_string(int64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static KString unsigned_to_string(uint64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void log_exception(const std::exception& e, const char* sWhere);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	static constexpr value_type s_0ch { '\0' };
	static value_type s_0ch_v;

	string_type m_rep;

}; // KString

DEKAF2_NAMESPACE_END

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// KString is now complete - here follow KString inline methods that need
// KStringView and / or KStringViewZ being complete as well

#include "kstringview.h"
#include "bits/kstringviewz.h"
#include "ksplit.h"
#include "kjoin.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
inline KString::KString(const KStringView& sv)
//-----------------------------------------------------------------------------
    : m_rep(sv.data(), sv.size())
{}

//-----------------------------------------------------------------------------
inline KString::KString(const KStringViewZ& svz)
//-----------------------------------------------------------------------------
	: m_rep(svz.data(), svz.size())
{}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type>
KString::KString(const T& sv)
//-----------------------------------------------------------------------------
: KString(KStringView(sv))
{
}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type>
KString::KString(const T& sv, size_type pos, size_type n)
//-----------------------------------------------------------------------------
: KString(KStringView(sv).substr(pos, n))
{
}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type>
KString& KString::assign(const T& sv)
//-----------------------------------------------------------------------------
{
	KStringView ksv(sv);
	m_rep.assign(ksv.data(), ksv.size());
	return *this;
}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type>
KString& KString::assign(const T& sv, size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	return assign(KStringView(sv).substr(pos, n));
}

//-----------------------------------------------------------------------------
inline KString& KString::operator= (value_type ch)
//-----------------------------------------------------------------------------
{
	return assign(1, ch);
}

//-----------------------------------------------------------------------------
inline KString& KString::operator= (const value_type *s)
//-----------------------------------------------------------------------------
{
	return assign(s);
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T>::value &&
		!std::is_same<T, std::string>::value
	, int>::type
>
KString& KString::operator= (const T& sv)
//-----------------------------------------------------------------------------
{
	return assign(KStringView(sv));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
KString& KString::replace(size_type pos, size_type n, T sv)
//-----------------------------------------------------------------------------
{
	return replace(pos, n, KStringView(sv));
}

//------------------------------------------------------------------------------
inline KString& KString::replace(size_type pos1, size_type n1, KStringView sv, size_type pos2, size_type n2)
//------------------------------------------------------------------------------
{
	return replace(pos1, n1, sv.substr(pos2, n2));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
KString& KString::replace(iterator i1, iterator i2, T sv)
//-----------------------------------------------------------------------------
{
	return replace(i1, i2, KStringView(sv));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
KString& KString::replace(size_type pos1, size_type n1, T sv, size_type pos2, size_type n2)
//-----------------------------------------------------------------------------
{
	return replace(pos1, n1, KStringView(sv).substr(pos2, n2));
}

//------------------------------------------------------------------------------
inline KString& KString::replace(size_type pos, size_type n1, const value_type* s, size_type n2)
//------------------------------------------------------------------------------
{
	return replace(pos, n1, KStringView(s, n2));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T>::value
	, int>::type
>
KString& KString::append(const T& sv)
//-----------------------------------------------------------------------------
{
	KStringView ksv(sv);
	m_rep.append(ksv.data(), ksv.size());
	return *this;
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T>::value
	, int>::type
>
KString& KString::append(const T& sv, size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	return append(KStringView(sv).substr(pos, n));
}

//-----------------------------------------------------------------------------
inline KString& KString::operator+= (std::initializer_list<value_type> il)
//-----------------------------------------------------------------------------
{
	return append(il);
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
KString& KString::operator+= (const T& sv)
//-----------------------------------------------------------------------------
{
	return append(KStringView(sv));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
int KString::compare(const T& sv) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(KStringView(sv));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
int KString::compare(size_type pos, size_type n1, const T& sv) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, KStringView(sv));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T>::value
	, int>::type
>
int KString::compare(size_type pos, size_type n1, const T& sv, size_type pos2, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, KStringView(sv), pos2, n2);
}

//-----------------------------------------------------------------------------
inline int KString::compare(size_type pos, size_type n1, const value_type* s, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, s, n2);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFind(KStringView(*this), c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFind(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(KStringView(s), pos);
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
inline KString::size_type KString::find(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find(s, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return m_rep.find(s, pos, n);
}

#endif

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
KString::size_type KString::find(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(KStringView(sv), pos);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kRFind(KStringView(*this), c, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kRFind(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(KStringView(s), pos);
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
inline KString::size_type KString::rfind(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.rfind(s, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::rfind(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return m_rep.rfind(s, pos, n);
}

#endif

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
KString::size_type KString::rfind(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(KStringView(sv), pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(c, pos);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindFirstOf(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_of(KStringView(s), pos);
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
inline KString::size_type KString::find_first_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_first_of(s, pos);
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

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
KString::size_type KString::find_first_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_of(KStringView(sv), pos);
}


//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(c, pos);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindLastOf(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_of(KStringView(s), pos);
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
inline KString::size_type KString::find_last_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_last_of(s, pos);
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

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
KString::size_type KString::find_last_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_of(KStringView(sv), pos);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(KStringView(s), pos);
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
	return kFindFirstNotOf(KStringView(*this), sv, pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_first_not_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_first_not_of(s, pos);
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

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
KString::size_type KString::find_first_not_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(KStringView(sv), pos);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindLastNotOf(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(KStringView(s), pos);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
inline KString::size_type KString::find_last_not_of(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find_last_not_of(s, pos);
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
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
KString::size_type KString::find_last_not_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(KStringView(sv), pos);
}

//------------------------------------------------------------------------------
inline KString& KString::insert(size_type pos1, KStringView sv, size_type pos2, size_type n)
//------------------------------------------------------------------------------
{
	return insert(pos1, sv.substr(pos2, n));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, true>::value
	, int>::type
>
KString& KString::insert(size_type pos, const T& sv)
//-----------------------------------------------------------------------------
{
	return insert(pos, KStringView(sv));
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T>::value
	, int>::type
>
KString& KString::insert(size_type pos1, const T& sv, size_type pos2, size_type n)
//-----------------------------------------------------------------------------
{
	return insert(pos1, KStringView(sv).substr(pos2, n));
}

//------------------------------------------------------------------------------
inline KString& KString::insert(size_type pos, const value_type* s, size_type n)
//------------------------------------------------------------------------------
{
	return insert(pos, KStringView(s, n));
}

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
inline KString::size_type KString::FindCaselessASCII(const KStringView sSearch, size_type pos) const noexcept
//-----------------------------------------------------------------------------
{
	return KStringView(*this).FindCaselessASCII(sSearch, pos);
}

//-----------------------------------------------------------------------------
inline bool KString::ContainsCaselessASCII(const KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return KStringView(*this).ContainsCaselessASCII(sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::starts_with(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kStartsWith(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::ends_with(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kEndsWith(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::starts_with(value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return empty() ? false : m_rep.front() == ch;
}

//-----------------------------------------------------------------------------
inline bool KString::ends_with(value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return empty() ? false : m_rep.back() == ch;
}

//-----------------------------------------------------------------------------
inline bool KString::contains(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::contains(const KString::value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), ch);
}

//-----------------------------------------------------------------------------
inline bool KString::StartsWith(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return starts_with(sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::EndsWith(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return ends_with(sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::Contains(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::Contains(const KString::value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), ch);
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpper() const
//-----------------------------------------------------------------------------
{
	return kToUpper(KStringView(*this));
}

//-----------------------------------------------------------------------------
inline KString KString::ToLower() const
//-----------------------------------------------------------------------------
{
	return kToLower(KStringView(*this));
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpperLocale() const &
//-----------------------------------------------------------------------------
{
	return kToUpperLocale(KStringView(*this));
}

//-----------------------------------------------------------------------------
inline KString KString::ToLowerLocale() const &
//-----------------------------------------------------------------------------
{
	return kToLowerLocale(KStringView(*this));
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpperASCII() const &
//-----------------------------------------------------------------------------
{
	return kToUpperASCII(KStringView(*this));
}

//-----------------------------------------------------------------------------
inline KString KString::ToLowerASCII() const &
//-----------------------------------------------------------------------------
{
	return kToLowerASCII(KStringView(*this));
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
inline KString::self&& KString::TrimLeft(KStringView sTrim) &&
//-----------------------------------------------------------------------------
{
	return std::move(TrimLeft(sTrim));
}

//-----------------------------------------------------------------------------
inline KString::self&& KString::TrimRight(KStringView sTrim) &&
//-----------------------------------------------------------------------------
{
	return std::move(TrimRight(sTrim));
}

//-----------------------------------------------------------------------------
inline KString::self&& KString::Trim(KStringView sTrim) &&
//-----------------------------------------------------------------------------
{
	return std::move(Trim(sTrim));
}

//-----------------------------------------------------------------------------
inline KString::self&& KString::Collapse(KStringView svCollapse, value_type chTo) &&
//-----------------------------------------------------------------------------
{
	return std::move(Collapse(svCollapse, chTo));
}

//-----------------------------------------------------------------------------
inline KString::self&& KString::CollapseAndTrim(KStringView svCollapse, value_type chTo) &&
//-----------------------------------------------------------------------------
{
	return std::move(CollapseAndTrim(svCollapse, chTo));
}

//-----------------------------------------------------------------------------
template<typename T, typename...Parms>
T KString::Split(Parms&&... parms) const
//-----------------------------------------------------------------------------
{
	T Container;
	kSplit(Container, *this, std::forward<Parms>(parms)...);
	return Container;
}

//-----------------------------------------------------------------------------
template<typename T, typename...Parms>
KString::self& KString::Join(const T& Container, Parms&&... parms) &
//-----------------------------------------------------------------------------
{
	kJoin(*this, Container, std::forward<Parms>(parms)...);
	return *this;
}

//-----------------------------------------------------------------------------
inline bool KString::Bool() const noexcept
//-----------------------------------------------------------------------------
{
	return ToView().Bool();
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
inline KStringView KString::MatchRegex(const KStringView sRegEx, size_type pos) const
//-----------------------------------------------------------------------------
{
	return ToView().MatchRegex(sRegEx, pos);
}

//-----------------------------------------------------------------------------
inline std::vector<KStringView> KString::MatchRegexGroups(const KStringView sRegEx, size_type pos) const
//-----------------------------------------------------------------------------
{
	return ToView().MatchRegexGroups(sRegEx, pos);
}

//-----------------------------------------------------------------------------
inline KStringView KString::Left(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().Left(iCount);
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::Mid(size_type iStart) const
//-----------------------------------------------------------------------------
{
	return ToView().Mid(iStart);
}

//-----------------------------------------------------------------------------
inline KStringView KString::Mid(size_type iStart, size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().Mid(iStart, iCount);
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::Right(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().Right(iCount);
}

//-----------------------------------------------------------------------------
inline KStringView KString::LeftUTF8(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().LeftUTF8(iCount);
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::MidUTF8(size_type iStart) const
//-----------------------------------------------------------------------------
{
	return ToView().MidUTF8(iStart);
}

//-----------------------------------------------------------------------------
inline KStringView KString::MidUTF8(size_type iStart, size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().MidUTF8(iStart, iCount);
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::RightUTF8(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().RightUTF8(iCount);
}

//-----------------------------------------------------------------------------
inline KCodePoint KString::AtUTF8(size_type iCount) const
//-----------------------------------------------------------------------------
{
	return ToView().AtUTF8(iCount);
}

//-----------------------------------------------------------------------------
inline bool KString::HasUTF8() const
//-----------------------------------------------------------------------------
{
	return ToView().HasUTF8();
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::SizeUTF8() const
//-----------------------------------------------------------------------------
{
	return ToView().SizeUTF8();
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::RemoveIllegalChars(KStringView sIllegalChars)
//-----------------------------------------------------------------------------
{
	return RemoveChars(sIllegalChars);
}

// KString inline methods until here
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//------------------------------------------------------------------------------
DEKAF2_PUBLIC
KStringRef::size_type kReplace(KStringRef& string,
							   const KStringView sSearch,
							   const KStringView sReplaceWith,
							   KStringRef::size_type pos = 0,
							   bool bReplaceAll = true);
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
DEKAF2_PUBLIC
KStringRef::size_type kReplace(KStringRef& string,
							   const KStringRef::value_type chSearch,
							   const KStringRef::value_type chReplaceWith,
							   KStringRef::size_type pos = 0,
							   bool bReplaceAll = true);
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// this inline method has to be defined after kReplace() ..
inline KString::size_type KString::Replace(const KStringView sSearch, const KStringView sReplace, size_type pos, bool bReplaceAll)
//-----------------------------------------------------------------------------
{
	return kReplace(*this, sSearch, sReplace, pos, bReplaceAll);
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

inline namespace literals {

	/// provide a string literal for KString
	inline DEKAF2_PREFIX KString operator"" _ks(const char *data, std::size_t size)
	{
		return {data, size};
	}

} // namespace literals

DEKAF2_NAMESPACE_END

#include "kformat.h"

DEKAF2_NAMESPACE_BEGIN

//----------------------------------------------------------------------
/// helper operator to allow KString as formatting arg of fmt::format
inline KString::operator DEKAF2_FORMAT_NAMESPACE ::string_view() const
//----------------------------------------------------------------------
{
	return DEKAF2_FORMAT_NAMESPACE::string_view(data(), size());
}

//----------------------------------------------------------------------
template<class... Args>
KString& KString::Format(Args&&... args) &
//----------------------------------------------------------------------
{
	*this = kFormat(std::forward<Args>(args)...);
	return *this;
}

DEKAF2_NAMESPACE_END

namespace std
{
	std::istream& getline(std::istream& stream, DEKAF2_PREFIX KString& str);
	std::istream& getline(std::istream& stream, DEKAF2_PREFIX KString& str, DEKAF2_PREFIX KString::value_type delimiter);

	/// provide a std::hash for KString
	template<> struct hash<DEKAF2_PREFIX KString>
	{
		// we actually use a KStringView as the parameter, as this avoids
		// accidentially constructing a KString if coming from a KStringView
		// or char* in a template that uses KString as elements
		DEKAF2_CONSTEXPR_14 std::size_t operator()(DEKAF2_PREFIX KStringView sv) const noexcept
		{
			return DEKAF2_PREFIX kHash(sv.data(), sv.size());
		}

		// and provide an explicit hash function for const char*, as this avoids
		// counting twice over the char array (KStringView's constructor counts
		// the size of the array)
		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return DEKAF2_PREFIX kHash(s);
		}
	};

	// make sure comparisons work without construction of KString
	template<> struct equal_to<DEKAF2_PREFIX KString>
	{
		using is_transparent = void;

		bool DEKAF2_CONSTEXPR_14 operator()(DEKAF2_PREFIX KStringView s1, DEKAF2_PREFIX KStringView s2) const
		{
			return s1 == s2;
		}
	};

	// make sure comparisons work without construction of KString
	template<> struct less<DEKAF2_PREFIX KString>
	{
		using is_transparent = void;

		bool DEKAF2_CONSTEXPR_17 operator()(DEKAF2_PREFIX KStringView s1, DEKAF2_PREFIX KStringView s2) const
		{
			return s1 < s2;
		}
	};

} // end of namespace std

#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
namespace boost {
#else
DEKAF2_NAMESPACE_BEGIN
#endif
	inline
	std::size_t hash_value(const DEKAF2_PREFIX KString& s)
	{
		return s.Hash();
	}
#ifdef BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP
}
#else
DEKAF2_NAMESPACE_END
#endif

//----------------------------------------------------------------------
inline std::size_t DEKAF2_PREFIX KString::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<DEKAF2_PREFIX KString>()(KStringView(*this));
}

//----------------------------------------------------------------------
inline std::size_t DEKAF2_PREFIX KString::CaseHash() const
//----------------------------------------------------------------------
{
	return DEKAF2_PREFIX kCaseHash(data(), size());
}

namespace DEKAF2_FORMAT_NAMESPACE {

template <>
struct formatter<DEKAF2_PREFIX KString> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const DEKAF2_PREFIX KString& String, FormatContext& ctx) const
	{
		return formatter<string_view>::format(string_view(String.data(), String.size()), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE
