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
#include "bits/khash.h"
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
template <class Value> class KStack;

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase (UTF8)
KString kToUpper(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase (UTF8)
KString kToLower(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
KString kToUpperLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
KString kToLowerLocale(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in uppercase assuming ASCII encoding
KString kToUpperASCII(KStringView sInput);
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// returns a copy of the string in lowercase assuming ASCII encoding
KString kToLowerASCII(KStringView sInput);
//----------------------------------------------------------------------

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// dekaf2's own string class - a wrapper around std::string or folly::fbstring
/// that offers most string functions from languages like Python or Javascript,
/// handles most error cases in a benign way and speeds up searching
/// up to 50 times compared to std::string implementations
class DEKAF2_GSL_OWNER() KString
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

	// Constructors
	KString ()                                                = default;
	KString (const KString& str)                              = default;
	KString (KString&& str) noexcept                          = default;
	KString (const KString& str, size_type pos, size_type n = npos) : m_rep(str.m_rep, (pos > str.size()) ? str.size() : pos, n) {}
	KString (KStringView sv);
	KString (KStringViewZ svz);
	KString (string_type sStr)                                : m_rep(std::move(sStr)) {}
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	KString (const std::string& sStr)                         : m_rep(sStr) {}
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	KString (const sv::string_view& str)                      : m_rep(str.data(), str.size()) {}
#endif
	KString (const value_type* s)                             : m_rep(s?s:"") {}

	KString (size_type n, value_type ch)                      : m_rep(n, ch) {}
	KString (const value_type* s, size_type n)                : m_rep(s ? s : "", s ? n : 0) {}
	KString (const value_type* s, size_type pos, size_type n) : m_rep(s ? s : "", s ? pos : 0, s ? n : 0) {}
	template<class _InputIterator>
	KString (_InputIterator first, _InputIterator last)       : m_rep(first, last) {}
	KString (std::initializer_list<value_type> il)            : m_rep(il) {}
#ifdef DEKAF2_KSTRING_HAS_ACQUIRE_MALLOCATED
	// nonstandard constructor to move an existing malloced buffer into the string
	KString (value_type *s, size_type n, size_type c, AcquireMallocatedString a) : m_rep(s, n, c, a) {}
#else
	// no buffer move possible, simply copy and release the input buffer
	KString (value_type *s, size_type n, size_type c, AcquireMallocatedString a) : KString (s, n) { if (s) delete(s); }
#endif

	self& operator= (const KString& str)                      = default;
	self& operator= (KString&& str) noexcept                  = default;
	self& operator= (const value_type* str)                   { return assign(str);          }
	self& operator= (value_type ch)                           { return assign(1, ch);        }
	self& operator= (std::initializer_list<value_type> il)    { return assign(il);           }

	self& operator+= (const KString& str)                     { return append(str);          }
	self& operator+= (const string_type& str)                 { return append(str);          }
	self& operator+= (const value_type ch)                    { push_back(ch); return *this; }
	self& operator+= (const value_type *s)                    { return append(s);            }
	self& operator+= (std::initializer_list<value_type> il)   { return append(il);           }
	self& operator+= (KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& operator+= (const std::string& str)                 { return append(str);          }
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	self& operator+= (sv::string_view str)                    { return append(str);          }
#endif

	iterator                begin()            noexcept       { return m_rep.begin();        }
	const_iterator          begin()            const noexcept { return m_rep.begin();        }
	iterator                end()              noexcept       { return m_rep.end();          }
	const_iterator          end()              const noexcept { return m_rep.end();          }
	reverse_iterator        rbegin()           noexcept       { return m_rep.rbegin();       }
	const_reverse_iterator  rbegin()           const noexcept { return m_rep.rbegin();       }
	reverse_iterator        rend()             noexcept       { return m_rep.rend();         }
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

	const_reference operator[](size_type pos)  const          { return at(pos);              }
	reference       operator[](size_type pos)                 { return at(pos);              }
	const_reference         at(size_type pos)  const          { if DEKAF2_UNLIKELY(pos >= size()) {                 return s_0ch;   } return m_rep.at(pos); }
	reference               at(size_type pos)                 { if DEKAF2_UNLIKELY(pos >= size()) { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.at(pos); }
	const_reference         back()             const          { if DEKAF2_UNLIKELY(empty())       {                 return s_0ch;   } return m_rep.back();  }
	reference               back()                            { if DEKAF2_UNLIKELY(empty())       { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.back();  }
	const_reference         front()            const          { if DEKAF2_UNLIKELY(empty())       {                 return s_0ch;   } return m_rep.front(); }
	reference               front()                           { if DEKAF2_UNLIKELY(empty())       { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.front(); }

	self& append(const KString& str)                          { m_rep.append(str.m_rep);            return *this; }
	self& append(const KString& str, size_type pos, size_type n = npos) { return append(str.m_rep, pos, n);       }
	self& append(const string_type& str)                      { m_rep.append(str);                  return *this; }
	self& append(const string_type& str, size_type pos, size_type n = npos);
	self& append(const value_type* str)                       { m_rep.append(str ? str : "");       return *this; }
	self& append(const value_type* str, size_type n)          { m_rep.append(str ? str : "", n);    return *this; }
	self& append(size_type n, value_type ch)                  { m_rep.append(n, ch);                return *this; }
	template<class _InputIterator>
	self& append(_InputIterator first, _InputIterator last)   { m_rep.append(first, last);          return *this; }
	self& append(std::initializer_list<value_type> il)        { m_rep.append(il);                   return *this; }
	self& append(KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& append(const std::string& str)                      { m_rep.append(str);                  return *this; }
	self& append(const std::string& str, size_type pos, size_type n = npos);
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	self& append(sv::string_view str)                         { return append(str.data(), str.size());            }
#endif

	self& push_back(const value_type chPushBack)              { m_rep.push_back(chPushBack);        return *this; }
	void pop_back()                                           { m_rep.pop_back();                                 }

	self& assign(const KString& str)                          { m_rep.assign(str.m_rep);            return *this; }
	self& assign(const KString& str, size_type pos, size_type n = npos) { return assign(str.m_rep, pos, n);       }
	self& assign(const string_type& str)                      { m_rep.assign(str);                  return *this; }
	self& assign(const string_type& str, size_type pos, size_type n = npos);
	self& assign(string_type&& str)                           { m_rep.assign(std::move(str));       return *this; }
	self& assign(const value_type* s, size_type n)            { m_rep.assign(s ? s : "", n);        return *this; }
	self& assign(const value_type* s)                         { m_rep.assign(s ? s : "");           return *this; }
	self& assign(size_type n, value_type ch)                  { m_rep.assign(n, ch);                return *this; }
	template<class _InputIterator>
	self& assign(_InputIterator first, _InputIterator last)   { m_rep.assign(first, last);          return *this; }
	self& assign(std::initializer_list<value_type> il)        { m_rep.assign(il);                   return *this; }
	self& assign(KString&& str)                               { m_rep.assign(std::move(str.m_rep)); return *this; }
	self& assign(KStringView sv);
	self& assign(KStringViewZ sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& assign(const std::string& str)                      { m_rep.assign(str);                  return *this; }
	self& assign(const std::string& str, size_type pos, size_type n = npos);
#endif
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	self& assign(sv::string_view sv)                          { return assign(sv.data(), sv.size());              }
#endif

	int compare(const KString& str)                                                const;
	int compare(size_type pos, size_type n, const KString& str)                    const;
	int compare(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos)     const;
	int compare(const string_type& str)                                            const;
	int compare(size_type pos, size_type n, const string_type& str)                const;
	int compare(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos) const;
	int compare(const value_type* s)                                               const;
	int compare(size_type pos, size_type n1, const value_type* s)                  const;
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2)    const;
	int compare(KStringView sv)                                                    const;
	int compare(size_type pos, size_type n1, KStringView sv)                       const;
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	int compare(const std::string& str)                                            const;
	int compare(size_type pos, size_type n, const std::string& str)                const;
	int compare(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos) const;
#endif

	size_type copy(value_type* s, size_type n, size_type pos = 0)                  const;

	size_type find(value_type c, size_type pos = 0)                                const;
	size_type find(KStringView sv, size_type pos = 0)                              const;
	size_type find(const value_type* s, size_type pos, size_type n)                const;
	size_type find(const KString& str, size_type pos = 0)                          const { return find(str.data(), pos, str.size());              }
	size_type find(const string_type& str, size_type pos = 0)                      const { return find(str.data(), pos, str.size());              }
	size_type find(const value_type* s, size_type pos = 0)                         const { return find(s, pos, strlen(s));                        }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find(const std::string& str, size_type pos = 0)                      const { return find(str.data(), pos, str.size());              }
#endif

	size_type rfind(value_type c, size_type pos = npos)                            const;
	size_type rfind(KStringView sv, size_type pos = npos)                          const;
	size_type rfind(const value_type* s, size_type pos, size_type n)               const;
	size_type rfind(const KString& str, size_type pos = npos)                      const { return rfind(str.data(), pos, str.size());             }
	size_type rfind(const string_type& str, size_type pos = npos)                  const { return rfind(str.data(), pos, str.size());             }
	size_type rfind(const value_type* s, size_type pos = npos)                     const { return rfind(s, pos, strlen(s));                       }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type rfind(const std::string& str, size_type pos = npos)                  const { return rfind(str.data(), pos, str.size());             }
#endif

	size_type find_first_of(value_type c, size_type pos = 0)                       const { return find(c, pos);                                   }
	size_type find_first_of(KStringView sv, size_type pos = 0)                     const;
	size_type find_first_of(const value_type* s, size_type pos, size_type n)       const;
	size_type find_first_of(const KString& str, size_type pos = 0)                 const { return find_first_of(str.data(), pos, str.size());     }
	size_type find_first_of(const string_type& str, size_type pos = 0)             const { return find_first_of(str.data(), pos, str.size());     }
	size_type find_first_of(const value_type* s, size_type pos = 0)                const { return find_first_of(s, pos, strlen(s));               }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_first_of(const std::string& str, size_type pos = 0)             const { return find_first_of(str.data(), pos, str.size());     }
#endif

	size_type find_last_of(value_type c, size_type pos = npos)                     const { return rfind(c, pos);                                  }
	size_type find_last_of(KStringView sv, size_type pos = npos)                   const;
	size_type find_last_of(const value_type* s, size_type pos, size_type n)        const;
	size_type find_last_of(const KString& str, size_type pos = npos)               const { return find_last_of(str.data(), pos, str.size());      }
	size_type find_last_of(const string_type& str, size_type pos = npos)           const { return find_last_of(str.data(), pos, str.size());      }
	size_type find_last_of(const value_type* s, size_type pos = npos)              const { return find_last_of(s, pos, strlen(s));                }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_last_of(const std::string& str, size_type pos = npos)           const { return find_last_of(str.data(), pos, str.size());      }
#endif

	size_type find_first_not_of(value_type c, size_type pos = 0)                   const { return find_first_not_of(&c, pos, 1);                  }
	size_type find_first_not_of(KStringView sv, size_type pos = 0)                 const;
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n)   const;
	size_type find_first_not_of(const KString& str, size_type pos = 0)             const { return find_first_not_of(str.data(), pos, str.size()); }
	size_type find_first_not_of(const string_type& str, size_type pos = 0)         const { return find_first_not_of(str.data(), pos, str.size()); }
	size_type find_first_not_of(const value_type* s, size_type pos = 0)            const { return find_first_not_of(s, pos, strlen(s));           }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_first_not_of(const std::string& str, size_type pos = 0)         const { return find_first_not_of(str.data(), pos, str.size()); }
#endif

	size_type find_last_not_of(value_type c, size_type pos = npos)                 const { return find_last_not_of(&c, pos, 1);                   }
	size_type find_last_not_of(KStringView sv, size_type pos = npos)               const;
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n)    const;
	size_type find_last_not_of(const KString& str, size_type pos = npos)           const { return find_last_not_of(str.data(), pos, str.size());  }
	size_type find_last_not_of(const string_type& str, size_type pos = npos)       const { return find_last_not_of(str.data(), pos, str.size());  }
	size_type find_last_not_of(const value_type* s, size_type pos = npos)          const { return find_last_not_of(s, pos, strlen(s));            }
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	size_type find_last_not_of(const std::string& str, size_type pos = npos)       const { return find_last_not_of(str.data(), pos, str.size());  }
#endif

	void insert(iterator p, size_type n, value_type c)                                   { m_rep.insert(p, n, c);                     }
	self& insert(size_type pos, const KString& str)                                      { return insert(pos, str.m_rep);             }
	self& insert(size_type pos1, const KString& str, size_type pos2, size_type n = npos) { return insert(pos1, str.m_rep, pos2, n);   }
	self& insert(size_type pos, const string_type& str);
	self& insert(size_type pos1, const string_type& str, size_type pos2, size_type n = npos);
	self& insert(size_type pos, const value_type* s, size_type n);
	self& insert(size_type pos, const value_type* s);
	self& insert(size_type pos, size_type n, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	iterator insert(iterator it, value_type c);
	template<class _InputIterator>
	void insert(const_iterator it, _InputIterator beg, _InputIterator end)              { m_rep.insert(it, beg, end);                 }
	// should be const_iterator with C++11, but is not supported by libstdc++
	self& insert (iterator it, std::initializer_list<value_type> il);
	self& insert(size_type pos, KStringView sv);
#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	self& insert(size_type pos, const std::string& str)                                 { return insert(pos, str.data(), str.size()); }
	self& insert(size_type pos1, const std::string& str, size_type pos2, size_type n = npos);
#endif

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

	self& replace(size_type pos, size_type n, const KString& str)                       { return replace(pos, n, str.m_rep); }
	self& replace(size_type pos1, size_type n1, const KString& str, size_type pos2, size_type n2 = npos) { return replace(pos1, n1, str.m_rep, pos2, n2); }
	self& replace(size_type pos, size_type n, const string_type& str);
	self& replace(size_type pos1, size_type n1, const string_type& str, size_type pos2, size_type n2 = npos);
	self& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	self& replace(size_type pos, size_type n1, const value_type* s);
	self& replace(size_type pos, size_type n1, size_type n2, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	self& replace(iterator i1, iterator i2, const KString& str)                         { return replace(i1, i2, str.m_rep); }
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
	self& Format(Args&&... args) &;

	/// print arguments with fmt::format
	template<class... Args>
	self&& Format(Args&&... args) && { return std::move(Format(std::forward<Args>(args)...)); }

	/// print arguments with fmt::printf - now DEPRECATED, please use Format()!
	template<class... Args>
	DEKAF2_DEPRECATED("only for compatibility with old code")
	self& Printf(Args&&... args);

	/// match with regular expression and return the overall match (group 0)
	KStringView MatchRegex(KStringView sRegEx, size_type pos = 0) const;

	/// match with regular expression and return all match groups
	std::vector<KStringView> MatchRegexGroups(KStringView sRegEx, size_type pos = 0) const;

	/// replace with regular expression, sReplaceWith may address sub-groups with \\1 etc., modifies string and returns number of replacements made
	size_type ReplaceRegex(KStringView sRegEx, KStringView sReplaceWith, bool bReplaceAll = true);

	/// replace one part of the string with another string, modifies string
	size_type Replace(KStringView sSearch, KStringView sReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace one char of the string with another char, modifies string and returns number of replacements made
	size_type Replace(value_type chSearch, value_type chReplace, size_type pos = 0, bool bReplaceAll = true);

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
	KString ToUpperLocale() const;

	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	KString ToLowerLocale() const;

	/// returns a copy of the string in uppercase assuming ASCII encoding
	KString ToUpperASCII() const;

	/// returns a copy of the string in lowercase assuming ASCII encoding
	KString ToLowerASCII() const;

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

	/// convert to representation type
	operator const string_type&()    const { return m_rep;               }
	operator string_type&()                { return m_rep;               }

#ifdef DEKAF2_USE_FBSTRING_AS_KSTRING
	/// convert to std::string
	std::string ToStdString()        const { return m_rep.toStdString(); }
#else
	/// convert to std::string
	const std::string& ToStdString() const { return m_rep;               }
#endif

	/// return the representation type
	const string_type& str()         const { return m_rep;               }

	string_type& str()                     { return m_rep;               }

	/// return a KStringViewZ
	KStringViewZ ToView() const;

	/// return a KStringViewZ much like a substr(), but without the cost
	KStringViewZ ToView(size_type pos) const;

	/// return a KStringView much like a substr(), but without the cost
	KStringView ToView(size_type pos, size_type n) const;

	/// test if the string is non-empty
	explicit operator bool() const { return !empty(); }

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
	static KString to_string(float f)
	//-----------------------------------------------------------------------------
	{
		return std::to_string(f);
	}

	//-----------------------------------------------------------------------------
	/// convert a double into a string
	static KString to_string(double f)
	//-----------------------------------------------------------------------------
	{
		return std::to_string(f);
	}

	//-----------------------------------------------------------------------------
	/// convert integer into a hex string
	static KString to_hexstring(uint64_t i, bool bZeroPad = true, bool bUpperCase = true)
	//-----------------------------------------------------------------------------
	{
		return unsigned_to_string(i, 16, bZeroPad, bUpperCase);
	}

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

} // end of namespace dekaf2


// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// KString is now complete - here follow KString inline methods that need
// KStringView and / or KStringViewZ being complete as well

#include "kstringview.h"
#include "bits/kstringviewz.h"
#include "ksplit.h"
#include "kjoin.h"

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
inline bool KString::starts_with(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kStartsWith(*this, sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::ends_with(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kEndsWith(*this, sSubString);
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
	return kContains(*this, sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::contains(const KString::value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(*this, ch);
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
	return kContains(*this, sSubString);
}

//-----------------------------------------------------------------------------
inline bool KString::Contains(const KString::value_type ch) const noexcept
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
inline KString KString::ToUpperASCII() const
//-----------------------------------------------------------------------------
{
	return kToUpperASCII(*this);
}

//-----------------------------------------------------------------------------
inline KString KString::ToLowerASCII() const
//-----------------------------------------------------------------------------
{
	return kToLowerASCII(*this);
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
inline KString::size_type KString::RemoveIllegalChars(KStringView sIllegalChars)
//-----------------------------------------------------------------------------
{
	return RemoveChars(sIllegalChars);
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
inline bool operator==(const KString& left, const sv::string_view& right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator==(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator==(const sv::string_view& left, const KString& right)
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
inline bool operator!=(const KString& left, const sv::string_view& right)
//-----------------------------------------------------------------------------
{
	return left.ToView().operator!=(KStringView(right));
}

//-----------------------------------------------------------------------------
inline bool operator!=(const sv::string_view& left, const KString& right)
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
KString& KString::Format(Args&&... args) &
//----------------------------------------------------------------------
{
	*this = kFormat(std::forward<Args>(args)...);
	return *this;
}

//----------------------------------------------------------------------
/// print arguments with fmt::printf - now DEPRECATED, please use Format()!
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
		DEKAF2_CONSTEXPR_14 std::size_t operator()(dekaf2::KStringView sv) const noexcept
		{
			return dekaf2::kHash(sv.data(), sv.size());
		}

		// and provide an explicit hash function for const char*, as this avoids
		// counting twice over the char array (KStringView's constructor counts
		// the size of the array)
		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return dekaf2::kHash(s);
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
#if (BOOST_VERSION < 106400)
	template<> struct hash<dekaf2::KString> : public std::unary_function<dekaf2::KString, std::size_t>
#else
	template<> struct hash<dekaf2::KString>
#endif
	{
		// we actually use a KStringView as the parameter, as this avoids
		// accidentially constructing a KString if coming from a KStringView
		// or char* in a template that uses KString as elements
		DEKAF2_CONSTEXPR_14 std::size_t operator()(dekaf2::KStringView s) const noexcept
		{
			return dekaf2::kHash(s.data(), s.size());
		}

		// and provide an explicit hash function for const char*, as this avoids
		// counting twice over the char array (KStringView's constructor counts
		// the size of the array)
		DEKAF2_CONSTEXPR_14 std::size_t operator()(const char* s) const noexcept
		{
			return dekaf2::kHash(s);
		}
	};

} // end of namespace boost

//----------------------------------------------------------------------
inline std::size_t dekaf2::KString::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<dekaf2::KString>()(*this);
}

