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
/// dekaf2's own string class - a safer, faster, and more convenient wrapper
/// around std::string that adds Python/JavaScript-style string operations,
/// handles error cases gracefully instead of causing UB, and speeds up
/// searching by up to 50x compared to typical std::string implementations

#include <dekaf2/kcompatibility.h>
#include <dekaf2/bits/kstring_view.h>
#include <dekaf2/ktemplate.h>
#include <dekaf2/kctype.h>
#include <dekaf2/bits/kformat.h>
#include <string>
#include <istream>
#include <ostream>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

// forward declarations
class KString;
class KStringView;
class KStringViewZ;
class KFindSetOfChars;
namespace kutf { template<typename Iterator> class CodepointRange; }

#if defined(DEKAF2_IS_APPLE_CLANG) && DEKAF2_CLANG_VERSION < 120000
/// A reference-compatible string type for use as output parameters.
/// On older Apple Clang (< 12) this is KString itself; on all other
/// compilers it is std::string, which converts to/from KString transparently.
/// Use `KStringRef&` in function signatures that write into a caller-provided string.
/// @ingroup core_strings
using KStringRef = KString;
#else
/// A reference-compatible string type for use as output parameters.
/// On older Apple Clang (< 12) this is KString itself; on all other
/// compilers it is std::string, which converts to/from KString transparently.
/// Use `KStringRef&` in function signatures that write into a caller-provided string.
/// @ingroup core_strings
using KStringRef = std::string;
#endif

namespace detail {

	using KStringStringType = std::string;

template<class T>
struct is_kstring_move_assignable
: std::integral_constant<
	bool,
	std::is_same<KString, T>::value ||
	std::is_same<KStringStringType, T>::value
> {};

} // end of namespace detail

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A safer and more convenient string class built on top of std::string.
///
/// KString wraps std::string and extends it with:
///
/// **Safety** - all element access (`operator[]`, `at()`, `front()`, `back()`)
/// is bounds-checked and returns a NUL character instead of causing undefined
/// behavior. `pop_back()` on an empty string is a no-op. Constructors accept
/// `nullptr` without crashing. The iterator constructor of std::string is
/// intentionally disabled (it silently causes buffer overflows on typos like
/// `std::string("one", "two")`).
///
/// **Convenience** - adds Python/JavaScript-style operations:
/// `Left()`, `Mid()`, `Right()`, `Split()`, `Join()`, `Trim()`,
/// `Collapse()`, `Replace()`, `Contains()`, `starts_with()`, `ends_with()`,
/// `ToUpper()`, `ToLower()`, `Bool()`, `Int64()`, `Format()`, and more.
/// Full UTF-8 support through `LeftUTF8()`, `MidUTF8()`, `RightUTF8()`,
/// `SizeUTF8()`, `AtUTF8()`, `Codepoints()` etc.
///
/// **Performance** - searching (`find()`, `rfind()`, `find_first_of()` etc.)
/// uses optimized algorithms that are up to 50x faster than typical
/// std::string implementations on glibc.
///
/// **Interoperability** - implicitly converts to/from `std::string`,
/// `KStringView`, `KStringViewZ`, `std::string_view`, and `fmt::string_view`.
/// Provides `std::hash`, `boost::hash`, and `fmt::formatter` specializations.
///
/// Usage:
/// @code
/// KString s = "Hello World";
/// s.MakeUpper();                          // "HELLO WORLD"
/// s.Replace("WORLD", "dekaf2");           // "HELLO dekaf2"
/// auto words = s.Split(" ");              // per default into a <std::vector<KStringView>>
/// bool has = s.contains("dekaf2");        // true
/// int  n   = KString("42").Int32();       // 42
/// auto fmt = KString().Format("{} {}", "Hello", 42); // "Hello 42"
/// @endcode
/// @ingroup core_strings
class DEKAF2_PUBLIC DEKAF2_GSL_OWNER(char) KString
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using string_type = detail::KStringStringType;
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

	/// @name Constructors
	/// @{

	/// default constructor - creates an empty string
	KString ()                                                = default;
	/// copy constructor
	KString (const KString& str)                              = default;
	/// move constructor
	KString (KString&& str) noexcept                          = default;

	/// construct from a C string (nullptr-safe: treated as empty string)
	DEKAF2_CONSTEXPR_STRING
	KString (const value_type* s)                             : m_rep(s?s:"") {}
	/// construct from a std::string
	DEKAF2_CONSTEXPR_STRING
	KString (const std::string& s)                            : m_rep(s.data(), s.size()) {}
	/// move-construct from a std::string
	DEKAF2_CONSTEXPR_STRING
	KString (std::string&& s) noexcept                        : m_rep(std::move(s)) {}
#ifdef DEKAF2_HAS_STD_STRING_VIEW
	DEKAF2_CONSTEXPR_STRING
	KString (const DEKAF2_SV_NAMESPACE::string_view& s)       : m_rep(s.data(), s.size()) {}
#endif
	/// construct from a KStringView
	DEKAF2_CONSTEXPR_STRING
	KString (const KStringView& sv);
	/// construct from a KStringViewZ
	DEKAF2_CONSTEXPR_STRING
	KString (const KStringViewZ& svz);
	/// construct from any type implicitly convertible to KStringView (explicit)
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	explicit KString (const T& sv);

	/// construct a substring of another KString from pos for n chars (bounds-safe)
	DEKAF2_CONSTEXPR_STRING
	KString (const KString& str, size_type pos, size_type n = npos) : m_rep(str.m_rep, (pos > str.size()) ? str.size() : pos, n) {}
	/// construct a string of n copies of character ch
	DEKAF2_CONSTEXPR_STRING
	KString (size_type n, value_type ch)                      : m_rep(n, ch) {}
	/// construct from a C string with explicit length (nullptr-safe)
	DEKAF2_CONSTEXPR_STRING
	KString (const value_type* s, size_type n)                : m_rep(s ? s : "", s ? n : 0) {}

	// the iterator constructor of std::string is way too dangerous -
	// a simple std::string("one", "two") causes a buffer overflow -
	// we will not implement it for KString..
	template<class _InputIterator>
	KString (_InputIterator first, _InputIterator last)       : m_rep(first, last) 
	{
		static_assert(first == last, "the iterator constructor is not supported - please use the (const char*, std::size_t) constructor instead");
	}

	/// construct from an initializer list of characters
	DEKAF2_CONSTEXPR_STRING
	KString (std::initializer_list<value_type> il)            : m_rep(il) {}
	/// construct a substring of a string-view-compatible type from pos for n chars
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	KString (const T& sv, size_type pos, size_type n);

	/// @}

	/// @name Assignment
	/// @{

	/// copy-and-swap assignment (handles both copy and move)
	DEKAF2_CONSTEXPR_STRING
	self& operator= (KString str) noexcept                    { swap(str); return *this;     }
	/// assign a single character
	DEKAF2_CONSTEXPR_STRING
	self& operator= (value_type ch);
	/// assign from a C string
	DEKAF2_CONSTEXPR_STRING
	self& operator= (const value_type *s);
	template<typename T,
	    typename std::enable_if<
			detail::is_kstringview_assignable<T>::value &&
			!std::is_same<T, std::string>::value
		, int>::type = 0
	>
	DEKAF2_CONSTEXPR_STRING
	self& operator= (const T& sv);
	self& operator= (std::nullptr_t) = delete; // C++23..

	/// @}

	/// @name Append operators
	/// @{

	/// append a string-view-compatible type
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& operator+= (const T& sv);
	/// append a single character
	DEKAF2_CONSTEXPR_STRING
	self& operator+= (const value_type ch)                    { push_back(ch); return *this; }
	/// append from an initializer list of characters
	DEKAF2_CONSTEXPR_STRING
	self& operator+= (std::initializer_list<value_type> il);

	/// @}

	/// @name Iterators
	/// @{

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	iterator                begin()                  noexcept { return m_rep.begin();        }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_iterator          begin()            const noexcept { return m_rep.begin();        }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	iterator                end()                    noexcept { return m_rep.end();          }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_iterator          end()              const noexcept { return m_rep.end();          }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	reverse_iterator        rbegin()                 noexcept { return m_rep.rbegin();       }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reverse_iterator  rbegin()           const noexcept { return m_rep.rbegin();       }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	reverse_iterator        rend()                   noexcept { return m_rep.rend();         }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reverse_iterator  rend()             const noexcept { return m_rep.rend();         }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_iterator          cbegin()           const noexcept { return m_rep.cbegin();       }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_iterator          cend()             const noexcept { return m_rep.cend();         }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reverse_iterator  crbegin()          const noexcept { return m_rep.crbegin();      }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reverse_iterator  crend()            const noexcept { return m_rep.crend();        }

	/// @}

	/// @name Data access
	/// @{

	/// returns a pointer to a null-terminated character array
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const value_type*       c_str()            const noexcept { return m_rep.c_str();        }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const value_type*       data()             const noexcept { return m_rep.data();         }
	// C++17 supports non-const data(), but gcc does not yet know it..
	/// returns a mutable pointer to the underlying character data
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	value_type*             data()                   noexcept { return &m_rep[0];            }

	/// @}

	/// @name Size and capacity
	/// @{

	/// returns the number of characters (bytes, not codepoints - see SizeUTF8())
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type               size()             const          { return m_rep.size();         }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type               length()           const          { return m_rep.length();       }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type               max_size()         const          { return m_rep.max_size();     }
	DEKAF2_CONSTEXPR_STRING
	void                    resize(size_type n, value_type c) { m_rep.resize(n, c);          }
	DEKAF2_CONSTEXPR_STRING
	void                    resize(size_type n)               { m_rep.resize(n);             }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type               capacity()         const          { return m_rep.capacity();     }
	DEKAF2_CONSTEXPR_STRING
	void                    reserve(size_type res_arg = 0)    { m_rep.reserve(res_arg);      }
	DEKAF2_CONSTEXPR_STRING
	void                    clear()                           { m_rep.clear();               }
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool                    empty()            const          { return m_rep.empty();        }
	DEKAF2_CONSTEXPR_STRING
	void                    shrink_to_fit()                   { m_rep.shrink_to_fit();       }
	/// resize the string to n characters without initializing new storage (performance optimization)
	void                    resize_uninitialized(size_type n);
	/// returns a hash value for the string content
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	std::size_t             Hash()             const;
	/// returns a case-insensitive hash value for the string content
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	std::size_t             CaseHash()         const;

	/// @}

	/// @name Element access
	/// All element access is bounds-checked: out-of-range access returns a NUL
	/// character instead of causing undefined behavior.
	/// @{

	/// bounds-checked element access (returns NUL on out-of-range)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reference operator[](size_type pos)  const          { return at(pos);              }
	/// bounds-checked mutable element access (returns mutable NUL on out-of-range)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	reference operator[](size_type pos)                       { return at(pos);              }
	/// bounds-checked element access (returns NUL on out-of-range, unlike std::string which has UB)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reference         at(size_type pos)  const          { if DEKAF2_UNLIKELY(pos >= size()) {                 return s_0ch;   } return m_rep[pos];    }
	/// bounds-checked mutable element access (returns mutable NUL on out-of-range)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	reference               at(size_type pos)                 { if DEKAF2_UNLIKELY(pos >= size()) { s_0ch_v = '\0'; return s_0ch_v; } return m_rep[pos];    }
	/// returns the last character, or NUL if empty (bounds-safe)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reference         back()             const          { if DEKAF2_UNLIKELY(empty())       {                 return s_0ch;   } return m_rep.back();  }
	/// returns a mutable reference to the last character, or mutable NUL if empty (bounds-safe)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	reference               back()                            { if DEKAF2_UNLIKELY(empty())       { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.back();  }
	/// returns the first character, or NUL if empty (bounds-safe)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const_reference         front()            const          { if DEKAF2_UNLIKELY(empty())       {                 return s_0ch;   } return m_rep.front(); }
	/// returns a mutable reference to the first character, or mutable NUL if empty (bounds-safe)
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	reference               front()                           { if DEKAF2_UNLIKELY(empty())       { s_0ch_v = '\0'; return s_0ch_v; } return m_rep.front(); }

	/// @}

	/// @name Modifiers
	/// @{

	/// append a C string (nullptr-safe)
	DEKAF2_CONSTEXPR_STRING
	self& append(const value_type* str)                       { m_rep.append(str ? str : "");       return *this; }
	/// append n characters from a C string (nullptr-safe)
	DEKAF2_CONSTEXPR_STRING
	self& append(const value_type* str, size_type n)          { m_rep.append(str ? str : "", n);    return *this; }
	/// append n copies of character ch
	DEKAF2_CONSTEXPR_STRING
	self& append(size_type n, value_type ch)                  { m_rep.append(n, ch);                return *this; }
	template<class _InputIterator>
	DEKAF2_CONSTEXPR_STRING
	self& append(_InputIterator first, _InputIterator last)   { m_rep.append(first, last);          return *this; }
	DEKAF2_CONSTEXPR_STRING
	self& append(std::initializer_list<value_type> il)        { m_rep.append(il);                   return *this; }
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& append(const T& sv);
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& append(const T& sv, size_type pos, size_type n = npos);

	/// append a single character
	DEKAF2_CONSTEXPR_STRING
	void  push_back(const value_type chPushBack)              { m_rep.push_back(chPushBack);                      }
	/// remove the last character (no-op if empty, unlike std::string which has UB)
	DEKAF2_CONSTEXPR_STRING
	void  pop_back()                                          { if DEKAF2_LIKELY(!empty()) { m_rep.pop_back(); }  }

	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& assign(const T& sv);
	DEKAF2_CONSTEXPR_STRING
	self& assign(size_type n, value_type ch)                  { m_rep.assign(n, ch);                return *this; }
	template<class _InputIterator>
	DEKAF2_CONSTEXPR_STRING
	self& assign(_InputIterator first, _InputIterator last)   { m_rep.assign(first, last);          return *this; }
	DEKAF2_CONSTEXPR_STRING
	self& assign(std::initializer_list<value_type> il)        { m_rep.assign(il);                   return *this; }
	DEKAF2_CONSTEXPR_STRING
	self& assign(KString&& str) noexcept                      { m_rep.assign(std::move(str.m_rep)); return *this; }
	DEKAF2_CONSTEXPR_STRING
	self& assign(const value_type* s, size_type n)            { m_rep.assign(s ? s : "", n);        return *this; }
	template<typename T,
	         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& assign(const T& sv, size_type pos, size_type n = npos);

	/// @name Comparison
	/// @{

	/// three-way lexicographic comparison with a string-view-compatible type
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	int compare(const T& sv)                                                       const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	int compare(size_type pos, size_type n1, const T& sv)                          const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	int compare(size_type pos, size_type n1, const value_type* s, size_type n2)    const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	int compare(size_type pos, size_type n1, const T& sv, size_type pos2, size_type n2 = npos) const;

	/// copy up to n characters starting at pos into the buffer pointed to by s
	DEKAF2_CONSTEXPR_STRING_TODO
	size_type copy(value_type* s, size_type n, size_type pos = 0)                  const;

	/// @}

	/// @name Search
	/// Uses optimized search algorithms (up to 50x faster than std::string on glibc), using SIMD where available.
	/// All search methods return npos if not found.
	/// @{

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find(value_type c, size_type pos = 0)                                const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find(KStringView sv, size_type pos = 0)                              const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find(const value_type* s, size_type pos = 0)                         const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find(const value_type* s, size_type pos, size_type n)                const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find(const T& sv, size_type pos = 0)                                 const;

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type rfind(value_type c, size_type pos = npos)                            const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type rfind(KStringView sv, size_type pos = npos)                          const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type rfind(const value_type* s, size_type pos = npos)                     const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type rfind(const value_type* s, size_type pos, size_type n)               const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type rfind(const T& sv, size_type pos = npos)                             const;

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_first_of(value_type c, size_type pos = 0)                       const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find_first_of(const KFindSetOfChars& CharSet, size_type pos = 0)     const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_first_of(const value_type* s, size_type pos, size_type n)       const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_first_of(const T& sv, size_type pos = 0)                        const;

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find_last_of(value_type c, size_type pos = npos)                     const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find_last_of(const KFindSetOfChars& CharSet, size_type pos = npos)   const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_last_of(const value_type* s, size_type pos, size_type n)        const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_last_of(const T& sv, size_type pos = npos)                      const;

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_first_not_of(value_type c, size_type pos = 0)                   const { return find_first_not_of(&c, pos, 1);                  }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find_first_not_of(const KFindSetOfChars& CharSet, size_type pos = 0) const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_first_not_of(const value_type* s, size_type pos, size_type n)   const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_first_not_of(const T& sv, size_type pos = 0)                    const;

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_last_not_of(value_type c, size_type pos = npos)                 const { return find_last_not_of(&c, pos, 1);                   }
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING_TODO
	size_type find_last_not_of(const KFindSetOfChars& CharSet, size_type pos = npos) const;
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_last_not_of(const value_type* s, size_type pos, size_type n)    const;
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, false>::value, int>::type = 0>
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	size_type find_last_not_of(const T& sv, size_type pos = npos)                  const;

	/// @}

	/// @name Insert and erase
	/// @{

	/// insert n copies of character c before iterator position p
	DEKAF2_CONSTEXPR_STRING
	void  insert(iterator p, size_type n, value_type c)                                  { m_rep.insert(p, n, c);                                 }
	DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	self& insert(size_type pos, KStringView sv);
	DEKAF2_CONSTEXPR_STRING_TODO
	self& insert(size_type pos1, KStringView sv, size_type pos2, size_type n = npos);
	DEKAF2_CONSTEXPR_STRING_TODO
	self& insert(size_type pos, const value_type* s, size_type n);
	DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	self& insert(size_type pos, size_type n, value_type c);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	iterator insert(iterator it, value_type c);
	template<class _InputIterator>
	DEKAF2_CONSTEXPR_STRING
	void insert(const_iterator it, _InputIterator beg, _InputIterator end)               { m_rep.insert(it, beg, end);                            }
	// should be const_iterator with C++11, but is not supported by libstdc++
	DEKAF2_CONSTEXPR_STRING_TODO
	iterator insert (iterator it, std::initializer_list<value_type> il);
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& insert(size_type pos, const T& sv);
	template<typename T,
			 typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type = 0>
	DEKAF2_CONSTEXPR_STRING
	self& insert(size_type pos1, const T& sv, size_type pos2, size_type n = npos);

	/// erase n characters starting at pos (graceful: clamps to valid range)
	DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	self& erase(size_type pos = 0, size_type n = npos);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	iterator erase(iterator position);
	// C++17 wants a const_iterator here, but the COW string implementation in libstdc++ does not have it
	DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	iterator erase(iterator first, iterator last);

	/// @}

	/// @name Prefix and suffix removal
	/// Borrowed from std::string_view for convenience on mutable strings.
	/// @{

	/// remove the last n characters (clamps to size if n > size())
	void remove_suffix(size_type n);
	/// remove the first n characters
	void remove_prefix(size_type n);
	/// remove a trailing substring if it matches; returns true if removed
	bool remove_suffix(KStringView suffix);
	/// remove a leading substring if it matches; returns true if removed
	bool remove_prefix(KStringView prefix);
	/// remove a leading character if it matches; returns true if removed
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
	/// remove a trailing character if it matches; returns true if removed
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

	/// @}

	/// @name Replace
	/// @{

	self& replace(size_type pos, size_type n, KStringView sv);
	self& replace(size_type pos1, size_type n1, KStringView sv, size_type pos2, size_type n2);
	self& replace(iterator i1, iterator i2, KStringView sv);
	self& replace(size_type pos, size_type n1, const value_type* s, size_type n2);
	self& replace(size_type pos, size_type n1, size_type n2, value_type c);
	template<class _InputIterator>
	DEKAF2_CONSTEXPR_STRING
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

	/// @}

	/// @name Substring
	/// @{

	/// substring starting at zero-based position "pos" for "n" chars.  if "n" is not specified return the rest of the string starting at "pos"
	DEKAF2_NODISCARD
	KString substr(size_type pos = 0, size_type n = npos) const &;

	/// substring starting at zero-based position "pos" for "n" chars.  if "n" is not specified return the rest of the string starting at "pos"
	DEKAF2_NODISCARD
	KString substr(size_type pos = 0, size_type n = npos) &&;

	/// swap contents with another KString
	DEKAF2_CONSTEXPR_STRING
	void swap(KString& other)     { using std::swap; swap(m_rep, other.m_rep); }
	/// swap contents with a std::string
	DEKAF2_CONSTEXPR_STRING
	void swap(std::string& other) { using std::swap; swap(m_rep, other);       }

	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	allocator_type get_allocator() const noexcept { return m_rep.get_allocator(); }

	/// @}

	/// @name Formatting
	/// @{

	/// print arguments with fmt::format
	template<class... Args>
	self& Format(KFormatString<Args...> sFormat, Args&&... args) &;

	/// print arguments with fmt::format
	template<class... Args>
	self&& Format(KFormatString<Args...> sFormat, Args&&... args) && { return std::move(Format(sFormat, std::forward<Args>(args)...)); }

	/// @}

	/// @name Regular expressions
	/// @{

	/// match with regular expression and return the overall match (group 0)
	/// please note the following flags when added to the search regex:
	/// (?i) : case insensitive compare.
	/// (?m) : multiline mode, ^ and $ will match any linefeed <br>instead of only start and end of the string.
	/// (?s) : let . match linefeed.
	/// (?U) : ungreedy.
	/// these flags can be combined like (?im) and have effect on the following string, they can be
	/// unset with a prepended - , like (?-im)
	DEKAF2_NODISCARD
	KStringView MatchRegex(const KStringView sRegEx, size_type pos = 0) const;

	/// match with regular expression and return all match groups
	/// please note the following flags when added to the search regex:
	/// (?i) : case insensitive compare.
	/// (?m) : multiline mode, ^ and $ will match any linefeed <br>instead of only start and end of the string.
	/// (?s) : let . match linefeed.
	/// (?U) : ungreedy.
	/// these flags can be combined like (?im) and have effect on the following string, they can be
	/// unset with a prepended - , like (?-im)
	DEKAF2_NODISCARD
	std::vector<KStringView> MatchRegexGroups(const KStringView sRegEx, size_type pos = 0) const;

	/// replace with regular expression, sReplaceWith may address sub-groups with \\1 etc., modifies string and returns number of replacements made
	/// please note the following flags when added to the search regex:
	/// (?i) : case insensitive compare.
	/// (?m) : multiline mode, ^ and $ will match any linefeed <br>instead of only start and end of the string.
	/// (?s) : let . match linefeed.
	/// (?U) : ungreedy.
	/// these flags can be combined like (?im) and have effect on the following string, they can be
	/// unset with a prepended - , like (?-im)
	size_type ReplaceRegex(const KStringView sRegEx, const KStringView sReplaceWith, bool bReplaceAll = true);

	/// @}

	/// @name High-level string operations
	/// @{

	/// replace part of the string with another string, modifies string and returns number of replacements made
	size_type Replace(const KStringView sSearch, const KStringView sReplace, size_type pos = 0, bool bReplaceAll = true);

	/// replace char of the string with another char, modifies string and returns number of replacements made
	size_type Replace(value_type chSearch, value_type chReplace, size_type pos = 0, bool bReplaceAll = true);

	/// find string with caseless ASCII compare
	DEKAF2_NODISCARD
	size_type FindCaselessASCII(const KStringView sSearch, size_type pos = 0) const noexcept;

	/// does the string contain sSubString (caseless ASCII compare)
	DEKAF2_NODISCARD
	bool ContainsCaselessASCII(const KStringView sSubString) const noexcept;

	// std::C++20
	/// does the string start with sSubString?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool starts_with(KStringView sSubString) const noexcept;

	// std::C++20
	/// does the string start with ch?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool starts_with(value_type	ch) const noexcept;

	// std::C++20
	/// does the string end with sSubString?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool ends_with(KStringView sSubString) const noexcept;

	// std::C++20
	/// does the string end with ch?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool ends_with(value_type ch) const noexcept;

	// std::C++23
	/// does the string contain the ch?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool contains(value_type ch) const noexcept;

	// std::C++23
	/// does the string contain the sSubString?
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING_TODO
	bool contains(KStringView sSubString) const noexcept;

	/// does the string start with sSubString? (Now deprecated, replace by starts_with())
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool StartsWith(KStringView sSubString) const noexcept;

	/// does the string end with sSubString? (Now deprecated, replace by ends_with())
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool EndsWith(KStringView sSubString) const noexcept;

	/// does the string contain the sSubString? (Now deprecated, replace by contains())
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING_TODO
	bool Contains(KStringView sSubString) const noexcept;

	/// does the string contain the ch? (Now deprecated, replace by contains())
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	bool Contains(value_type ch) const noexcept;

	/// @name Case conversion
	/// Three variants: UTF-8 aware (default), locale-based, and ASCII-only.
	/// `Make*` modifies in place, `To*` returns a copy.
	/// @{

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

	// note that it does not make sense to add an rvalue version of
	// ToUpper() and ToLower() as we always have to create a new string
	// (see also note in kstring.cpp about UTF8 casefolds)

	/// returns a copy of the string in uppercase (UTF8)
	DEKAF2_NODISCARD
	KString ToUpper() const;

	/// returns a copy of the string in lowercase (UTF8)
	DEKAF2_NODISCARD
	KString ToLower() const;

	/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
	DEKAF2_NODISCARD
	KString ToUpperLocale() const &;

	/// returns a copy of the string in uppercase according to the current locale (does not work with UTF8 strings)
	DEKAF2_NODISCARD
	self&& ToUpperLocale() && { return std::move(MakeUpperLocale()); }

	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	DEKAF2_NODISCARD
	KString ToLowerLocale() const &;

	/// returns a copy of the string in lowercase according to the current locale (does not work with UTF8 strings)
	DEKAF2_NODISCARD
	self&& ToLowerLocale() && { return std::move(MakeLowerLocale()); }

	/// returns a copy of the string in uppercase assuming ASCII encoding
	DEKAF2_NODISCARD
	KString ToUpperASCII() const &;

	/// returns a copy of the string in uppercase assuming ASCII encoding
	DEKAF2_NODISCARD
	self&& ToUpperASCII() && { return std::move(MakeUpperASCII()); }

	/// returns a copy of the string in lowercase assuming ASCII encoding
	DEKAF2_NODISCARD
	KString ToLowerASCII() const &;

	/// returns a copy of the string in lowercase assuming ASCII encoding
	DEKAF2_NODISCARD
	self&& ToLowerASCII() && { return std::move(MakeLowerASCII()); }

	/// @}

	/// @name Substrings (byte-based)
	/// Python-style Left/Mid/Right accessors. On lvalues they return
	/// lightweight KStringView/KStringViewZ; on rvalues they truncate in place.
	/// @{

	/// returns leftmost iCount chars of string
	DEKAF2_NODISCARD
	KStringView Left(size_type iCount) const &;

	/// returns leftmost iCount chars of string
	DEKAF2_NODISCARD
	self&& Left(size_type iCount) &&;

	/// returns substring starting at iStart until end of string
	DEKAF2_NODISCARD
	KStringViewZ Mid(size_type iStart) const &;

	/// returns substring starting at iStart until end of string
	DEKAF2_NODISCARD
	self&& Mid(size_type iStart) &&;

	/// returns substring starting at iStart for iCount chars
	DEKAF2_NODISCARD
	KStringView Mid(size_type iStart, size_type iCount) const &;

	/// returns substring starting at iStart for iCount chars
	DEKAF2_NODISCARD
	self&& Mid(size_type iStart, size_type iCount) &&;

	/// returns rightmost iCount chars of string
	DEKAF2_NODISCARD
	KStringViewZ Right(size_type iCount) const &;

	/// returns rightmost iCount chars of string
	DEKAF2_NODISCARD
	self&& Right(size_type iCount) &&;

	/// @}

	/// @name Substrings (UTF-8 codepoint-based)
	/// Like Left/Mid/Right but counting Unicode codepoints instead of bytes.
	/// @{

	/// returns leftmost iCount codepoints of string
	DEKAF2_NODISCARD
	KStringView LeftUTF8(size_type iCount) const &;

	/// returns leftmost iCount codepoints of string
	DEKAF2_NODISCARD
	self&& LeftUTF8(size_type iCount) &&;

	/// returns substring starting at codepoint iStart until end of string
	DEKAF2_NODISCARD
	KStringViewZ MidUTF8(size_type iStart) const &;

	/// returns substring starting at codepoint iStart until end of string
	DEKAF2_NODISCARD
	self&& MidUTF8(size_type iStart) &&;

	/// returns substring starting at codepoint iStart for iCount codepoints
	DEKAF2_NODISCARD
	KStringView MidUTF8(size_type iStart, size_type iCount) const &;

	/// returns substring starting at codepoint iStart for iCount codepoints
	DEKAF2_NODISCARD
	self&& MidUTF8(size_type iStart, size_type iCount) &&;

	/// returns rightmost codepoints chars of string
	DEKAF2_NODISCARD
	KStringViewZ RightUTF8(size_type iCount) const &;

	/// returns rightmost codepoints chars of string
	DEKAF2_NODISCARD
	self&& RightUTF8(size_type iCount) &&;

	/// returns KCcodePoint at UTF8 position iCount
	DEKAF2_NODISCARD
	KCodePoint AtUTF8(size_type iCount) const;

	/// returns true if string contains UTF8 runs
	DEKAF2_NODISCARD
	bool HasUTF8() const;

	/// returns the count of unicode codepoints (or, UTF8 sequences)
	DEKAF2_NODISCARD
	size_type SizeUTF8() const;

	/// returns a range for forward iteration over Unicode codepoints (UTF8 decoding)
	DEKAF2_NODISCARD
	kutf::CodepointRange<const_iterator> Codepoints() const;

	/// @}

	/// @name Padding, trimming, and collapsing
	/// @{

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
	self& TrimLeft(const KFindSetOfChars& sTrim) &;
	/// removes any character in sTrim from the left of the string
	self&& TrimLeft(const KFindSetOfChars& sTrim) &&;

	/// removes white space from the right of the string
	self& TrimRight() &;
	/// removes white space from the right of the string
	self&& TrimRight() && { return std::move(TrimRight()); }
	/// removes chTrim from the right of the string
	self& TrimRight(value_type chTrim) &;
	/// removes chTrim from the right of the string
	self&& TrimRight(value_type chTrim) && { return std::move(TrimRight(chTrim)); }
	/// removes any character in sTrim from the right of the string
	self& TrimRight(const KFindSetOfChars& sTrim)&;
	/// removes any character in sTrim from the right of the string
	self&& TrimRight(const KFindSetOfChars& sTrim) &&;

	/// removes white space from the left and right of the string
	self& Trim() &;
	/// removes white space from the left and right of the string
	self&& Trim() && { return std::move(Trim()); }
	/// removes chTrim from the left and right of the string
	self& Trim(value_type chTrim) &;
	/// removes chTrim from the left and right of the string
	self&& Trim(value_type chTrim) && { return std::move(Trim(chTrim)); }
	/// removes any character in sTrim from the left and right of the string
	self& Trim(const KFindSetOfChars& sTrim) &;
	/// removes any character in sTrim from the left and right of the string
	self&& Trim(const KFindSetOfChars& sTrim) &&;

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

	/// @}

	/// @name Clipping and character removal
	/// @{

	/// Clip removing sClipAt and everything to its right if found; otherwise do not alter the string
	bool ClipAt(KStringView sClipAt);

	/// Clip removing everything to the left of sClipAtReverse so that sClipAtReverse becomes the beginning of the string;
	/// otherwise do not alter the string
	bool ClipAtReverse(KStringView sClipAtReverse);

	/// Remove any occurence of the characters in sChars, returns count of removed chars
	size_type RemoveChars(KStringView sChars);

	/// Remove any occurence of chChar, returns count of removed chars
	size_type RemoveChars(value_type chChar);

	/// Deprecated alias for RemoveChars, please replace by RemoveChars()
	size_type RemoveIllegalChars(KStringView sIllegalChars);

	/// @}

	/// @name Split and join
	/// @{

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
	DEKAF2_NODISCARD
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

	/// @}

	/// @name Type conversions
	/// @{

	/// convert to std::string
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	const std::string& ToStdString() const & { return m_rep;             }
	/// convert to std::string
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	std::string& ToStdString() &           { return m_rep;               }
	/// convert to std::string
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	std::string&& ToStdString() &&         { return std::move(m_rep);    }

	/// return the string type
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	const string_type& str()       const & { return m_rep;               }
	/// return the string type non-const
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	string_type& str() &                   { return m_rep;               }
	/// return the string type as an rvalue
	DEKAF2_NODISCARD_PEDANTIC DEKAF2_CONSTEXPR_STRING
	string_type&& str() &&                 { return std::move(m_rep);    }

	/// return a KStringViewZ
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING
	KStringViewZ ToView() const;

	/// return a KStringViewZ much like a substr(), but without the cost
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	KStringViewZ ToView(size_type pos) const;

	/// return a KStringView much like a substr(), but without the cost
	DEKAF2_NODISCARD DEKAF2_CONSTEXPR_STRING_TODO_MAKE_INLINE
	KStringView ToView(size_type pos, size_type n) const;

	/// test if the string is non-empty
	DEKAF2_CONSTEXPR_STRING
	explicit operator bool()      const    { return !empty();            }

	/// return the string type
	DEKAF2_CONSTEXPR_STRING
	operator const string_type&() const &  { return m_rep;               }
	/// return the string type non-const
	DEKAF2_CONSTEXPR_STRING
	operator string_type&() &              { return m_rep;               }
	/// return the string type as an rvalue
	DEKAF2_CONSTEXPR_STRING
	operator string_type&&() &&            { return std::move(m_rep);    }

#if DEKAF2_HAS_FMT_FORMAT
	/// helper operator to allow KString as formatting arg of fmt::format
	DEKAF2_CONSTEXPR_STRING
	operator DEKAF2_FORMAT_NAMESPACE::string_view() const;
#endif

#ifdef DEKAF2_HAS_STD_STRING_VIEW
	/// allowing conversion into std::string_view
	DEKAF2_CONSTEXPR_STRING
	operator DEKAF2_SV_NAMESPACE::string_view() const { return DEKAF2_SV_NAMESPACE::string_view(m_rep.data(), m_rep.size()); }
#endif
	/// is string one of the values in sHaystack, delimited by iDelim?
	DEKAF2_NODISCARD
	bool In (KStringView sHaystack, value_type iDelim=',') const;

	/// @}

	/// @name Numeric conversions
	/// Parse the string content as a number. Returns 0 on failure (no exceptions).
	/// @{

	/// returns bool representation of the string:
	/// "true" --> true
	/// "false" --> false
	/// as well as non-0 --> true
	DEKAF2_NODISCARD
	bool Bool() const noexcept;
	/// convert to int16_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int16_t Int16(bool bIsHex = false) const noexcept;
	/// convert to uint16_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint16_t UInt16(bool bIsHex = false) const noexcept;
	/// convert to int32_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int32_t Int32(bool bIsHex = false) const noexcept;
	/// convert to uint32_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint32_t UInt32(bool bIsHex = false) const noexcept;
	/// convert to int64_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int64_t Int64(bool bIsHex = false) const noexcept;
	/// convert to uint64_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint64_t UInt64(bool bIsHex = false) const noexcept;
#ifdef DEKAF2_HAS_INT128
	/// convert to int128_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	int128_t Int128(bool bIsHex = false) const noexcept;
	/// convert to uint128_t - set bIsHex to true if string is a hex number
	DEKAF2_NODISCARD
	uint128_t UInt128(bool bIsHex = false) const noexcept;
#endif
	/// convert to float
	DEKAF2_NODISCARD
	float Float() const noexcept;
	/// convert to double
	DEKAF2_NODISCARD
	double Double() const noexcept;

	//-----------------------------------------------------------------------------
	/// convert any integer to KString
	template<typename Integer,
	         typename std::enable_if<detail::is_duration<Integer>::value == false, int>::type = 0>
	DEKAF2_NODISCARD
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
	/// convert any duration to KString - note that this returns the pure tick count, and no unit whatsoever
	template<typename Duration,
	         typename std::enable_if<detail::is_duration<Duration>::value == true, int>::type = 0>
	DEKAF2_NODISCARD
	static KString to_string(Duration d)
	//-----------------------------------------------------------------------------
	{
		return to_string(d.count());
	}

	//-----------------------------------------------------------------------------
	/// convert a float into a string
	DEKAF2_NODISCARD
	static KString to_string(float f);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert a double into a string
	DEKAF2_NODISCARD
	static KString to_string(double f);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// convert an unsigned integer into a hex string (with optional zero-padding)
	DEKAF2_NODISCARD
	static KString to_hexstring(uint64_t i, bool bZeroPad = true, bool bUpperCase = true)
	//-----------------------------------------------------------------------------
	{
		return unsigned_to_string(i, 16, bZeroPad, bUpperCase);
	}

	/// @}

//----------
protected:
//----------

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	static KString signed_to_string(int64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	DEKAF2_NODISCARD
	static KString unsigned_to_string(uint64_t i, uint16_t iBase, bool bZeroPad, bool bUppercase);
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	static void log_exception(const std::exception& e, const char* sWhere);
	//-----------------------------------------------------------------------------

//----------
private:
//----------

	static constexpr value_type s_0ch { '\0' };
	static thread_local value_type s_0ch_v;

	string_type m_rep;

}; // KString

DEKAF2_NAMESPACE_END

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// KString is now complete - here follow KString inline methods that need
// KStringView and / or KStringViewZ being complete as well, or use functions
// from ksplit.h, kjoin.h, or kstringutils.h

#include "kstringview.h"
#include <dekaf2/bits/kstringviewz.h>
#include "ksplit.h"
#include "kjoin.h"
#include "kstringutils.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::KString(const KStringView& sv)
//-----------------------------------------------------------------------------
    : m_rep(sv.data(), sv.size())
{}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::KString(const KStringViewZ& svz)
//-----------------------------------------------------------------------------
	: m_rep(svz.data(), svz.size())
{}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T>::value, int>::type>
DEKAF2_CONSTEXPR_STRING
KString::KString(const T& sv)
//-----------------------------------------------------------------------------
: KString(KStringView(sv))
{
}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type>
DEKAF2_CONSTEXPR_STRING
KString::KString(const T& sv, size_type pos, size_type n)
//-----------------------------------------------------------------------------
: KString(KStringView(sv).substr(pos, n))
{
}

//-----------------------------------------------------------------------------
template<typename T,
         typename std::enable_if<detail::is_kstringview_assignable<T, true>::value, int>::type>
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
KString& KString::assign(const T& sv, size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	return assign(KStringView(sv).substr(pos, n));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString& KString::operator= (value_type ch)
//-----------------------------------------------------------------------------
{
	return assign(1, ch);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString& KString::operator= (const value_type *s)
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
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
KString& KString::append(const T& sv, size_type pos, size_type n)
//-----------------------------------------------------------------------------
{
	return append(KStringView(sv).substr(pos, n));
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString& KString::operator+= (std::initializer_list<value_type> il)
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
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
int KString::compare(size_type pos, size_type n1, const T& sv, size_type pos2, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, KStringView(sv), pos2, n2);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
int KString::compare(size_type pos, size_type n1, const value_type* s, size_type n2) const
//-----------------------------------------------------------------------------
{
	return ToView().compare(pos, n1, s, n2);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFind(KStringView(*this), c, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFind(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(KStringView(s), pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find(c, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(sv.data(), pos, sv.size());
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.find(s, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find(const value_type* s, size_type pos, size_type n) const
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
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(KStringView(sv), pos);
}

#if defined(DEKAF2_USE_OPTIMIZED_STRING_FIND)

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::rfind(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kRFind(KStringView(*this), c, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::rfind(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kRFind(KStringView(*this), sv, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::rfind(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(KStringView(s), pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::rfind(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return rfind(KStringView(s, n), pos);
}

#else

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::rfind(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.rfind(c, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::rfind(KStringView sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(sv.data(), pos, sv.size());
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::rfind(const value_type* s, size_type pos) const
//-----------------------------------------------------------------------------
{
	return m_rep.rfind(s, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::rfind(const value_type* s, size_type pos, size_type n) const
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
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::rfind(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(KStringView(sv), pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_first_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find(c, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find_first_of(const KFindSetOfChars& CharSet, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindFirstOf(KStringView(*this), CharSet, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_first_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_first_of(KStringView(s, n), pos);
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_first_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_of(KFindSetOfChars(KStringView(sv)), pos);
}


//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find_last_of(value_type c, size_type pos) const
//-----------------------------------------------------------------------------
{
	return rfind(c, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find_last_of(const KFindSetOfChars& CharSet, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindLastOf(KStringView(*this), CharSet, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_last_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_last_of(KStringView(s, n), pos);
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_last_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_of(KFindSetOfChars(KStringView(sv)), pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find_first_not_of(const KFindSetOfChars& CharSet, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindFirstNotOf(KStringView(*this), CharSet, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_first_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(KStringView(s, n), pos);
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_first_not_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_first_not_of(KFindSetOfChars(KStringView(sv)), pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString::size_type KString::find_last_not_of(const KFindSetOfChars& CharSet, size_type pos) const
//-----------------------------------------------------------------------------
{
	return kFindLastNotOf(KStringView(*this), CharSet, pos);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_last_not_of(const value_type* s, size_type pos, size_type n) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(KStringView(s, n), pos);
}

//-----------------------------------------------------------------------------
template<typename T,
	typename std::enable_if<
		detail::is_kstringview_assignable<T, false>::value
	, int>::type
>
DEKAF2_CONSTEXPR_STRING
KString::size_type KString::find_last_not_of(const T& sv, size_type pos) const
//-----------------------------------------------------------------------------
{
	return find_last_not_of(KFindSetOfChars(KStringView(sv)), pos);
}

//------------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString& KString::insert(size_type pos1, KStringView sv, size_type pos2, size_type n)
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
DEKAF2_CONSTEXPR_STRING
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
DEKAF2_CONSTEXPR_STRING
KString& KString::insert(size_type pos1, const T& sv, size_type pos2, size_type n)
//-----------------------------------------------------------------------------
{
	return insert(pos1, KStringView(sv).substr(pos2, n));
}

//------------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
KString& KString::insert(size_type pos, const value_type* s, size_type n)
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
DEKAF2_CONSTEXPR_STRING
bool KString::starts_with(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kStartsWith(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::ends_with(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kEndsWith(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::starts_with(value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return empty() ? false : m_rep.front() == ch;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::ends_with(value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return empty() ? false : m_rep.back() == ch;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
bool KString::contains(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::contains(const KString::value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), ch);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::StartsWith(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return starts_with(sSubString);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::EndsWith(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return ends_with(sSubString);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING_TODO
bool KString::Contains(KStringView sSubString) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), sSubString);
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
bool KString::Contains(const KString::value_type ch) const noexcept
//-----------------------------------------------------------------------------
{
	return kContains(KStringView(*this), ch);
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
inline KString& KString::MakeUpper() &
//-----------------------------------------------------------------------------
{
	kMakeUpper(*this);
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::MakeLower() &
//-----------------------------------------------------------------------------
{
	kMakeLower(*this);
	return *this;
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpperLocale() const &
//-----------------------------------------------------------------------------
{
	return ToView().ToUpperLocale();
}

//-----------------------------------------------------------------------------
inline KString KString::ToLowerLocale() const &
//-----------------------------------------------------------------------------
{
	return ToView().ToLowerLocale();
}

//-----------------------------------------------------------------------------
inline KString KString::ToUpperASCII() const &
//-----------------------------------------------------------------------------
{
	return ToView().ToUpperASCII();
}

//-----------------------------------------------------------------------------
inline KString KString::ToLowerASCII() const &
//-----------------------------------------------------------------------------
{
	return ToView().ToLowerASCII();
}

//-----------------------------------------------------------------------------
inline KString& KString::MakeUpperASCII() &
//-----------------------------------------------------------------------------
{
	kMakeUpperASCII(*this);
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::MakeLowerASCII() &
//-----------------------------------------------------------------------------
{
	kMakeLowerASCII(*this);
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::MakeUpperLocale() &
//-----------------------------------------------------------------------------
{
	kMakeUpperLocale(*this);
	return *this;
}

//-----------------------------------------------------------------------------
inline KString& KString::MakeLowerLocale() &
//-----------------------------------------------------------------------------
{
	kMakeLowerLocale(*this);
	return *this;
}

//-----------------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING
KStringViewZ KString::ToView() const
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
inline KString::self&& KString::TrimLeft(const KFindSetOfChars& TrimSet) &&
//-----------------------------------------------------------------------------
{
	return std::move(TrimLeft(TrimSet));
}

//-----------------------------------------------------------------------------
inline KString::self&& KString::TrimRight(const KFindSetOfChars& TrimSet) &&
//-----------------------------------------------------------------------------
{
	return std::move(TrimRight(TrimSet));
}

//-----------------------------------------------------------------------------
inline KString::self&& KString::Trim(const KFindSetOfChars& TrimSet) &&
//-----------------------------------------------------------------------------
{
	return std::move(Trim(TrimSet));
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
	T Container{};
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
inline KStringView KString::Left(size_type iCount) const &
//-----------------------------------------------------------------------------
{
	return ToView().Left(iCount);
}

//-----------------------------------------------------------------------------
inline KString&& KString::Left(size_type iCount) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeLeft(*this, iCount));
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::Mid(size_type iStart) const &
//-----------------------------------------------------------------------------
{
	return ToView().Mid(iStart);
}

//-----------------------------------------------------------------------------
inline KString&& KString::Mid(size_type iStart) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeMid(*this, iStart));
}

//-----------------------------------------------------------------------------
inline KStringView KString::Mid(size_type iStart, size_type iCount) const &
//-----------------------------------------------------------------------------
{
	return ToView().Mid(iStart, iCount);
}

//-----------------------------------------------------------------------------
inline KString&& KString::Mid(size_type iStart, size_type iCount) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeMid(*this, iStart, iCount));
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::Right(size_type iCount) const &
//-----------------------------------------------------------------------------
{
	return ToView().Right(iCount);
}

//-----------------------------------------------------------------------------
inline KString&& KString::Right(size_type iCount) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeRight(*this, iCount));
}

//-----------------------------------------------------------------------------
inline KStringView KString::LeftUTF8(size_type iCount) const &
//-----------------------------------------------------------------------------
{
	return ToView().LeftUTF8(iCount);
}

//-----------------------------------------------------------------------------
inline KString&& KString::LeftUTF8(size_type iCount) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeLeftUTF(*this, iCount));
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::MidUTF8(size_type iStart) const &
//-----------------------------------------------------------------------------
{
	return ToView().MidUTF8(iStart);
}

//-----------------------------------------------------------------------------
inline KString&& KString::MidUTF8(size_type iStart) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeMidUTF(*this, iStart));
}

//-----------------------------------------------------------------------------
inline KStringView KString::MidUTF8(size_type iStart, size_type iCount) const &
//-----------------------------------------------------------------------------
{
	return ToView().MidUTF8(iStart, iCount);
}

//-----------------------------------------------------------------------------
inline KString&& KString::MidUTF8(size_type iStart, size_type iCount) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeMidUTF(*this, iStart, iCount));
}

//-----------------------------------------------------------------------------
inline KStringViewZ KString::RightUTF8(size_type iCount) const &
//-----------------------------------------------------------------------------
{
	return ToView().RightUTF8(iCount);
}

//-----------------------------------------------------------------------------
inline KString&& KString::RightUTF8(size_type iCount) &&
//-----------------------------------------------------------------------------
{
	return std::move(kMakeRightUTF(*this, iCount));
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
inline kutf::CodepointRange<KString::const_iterator> KString::Codepoints() const
//-----------------------------------------------------------------------------
{
	return { cbegin(), cend() };
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::RemoveIllegalChars(KStringView sIllegalChars)
//-----------------------------------------------------------------------------
{
	return RemoveChars(sIllegalChars);
}

//-----------------------------------------------------------------------------
inline KString::size_type KString::Replace(const KStringView sSearch, const KStringView sReplace, size_type pos, bool bReplaceAll)
//-----------------------------------------------------------------------------
{
	return kReplace(*this, sSearch, sReplace, pos, bReplaceAll);
}

// KString inline methods until here
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
	KString temp;
	temp.reserve(left.size() + 1);
	temp += left;
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
	inline DEKAF2_PREFIX KString operator""_ks(const char *data, std::size_t size)
	{
		return {data, size};
	}

} // namespace literals

DEKAF2_NAMESPACE_END

#include <dekaf2/kformat.h>

DEKAF2_NAMESPACE_BEGIN

#if DEKAF2_HAS_FMT_FORMAT
//----------------------------------------------------------------------
/// helper operator to allow KString as formatting arg of fmt::format
DEKAF2_CONSTEXPR_STRING
KString::operator DEKAF2_FORMAT_NAMESPACE ::string_view() const
//----------------------------------------------------------------------
{
	return DEKAF2_FORMAT_NAMESPACE::string_view(data(), size());
}
#endif

//----------------------------------------------------------------------
template<class... Args>
KString& KString::Format(KFormatString<Args...> sFormat, Args&&... args) &
//----------------------------------------------------------------------
{
	*this = kFormat(sFormat, std::forward<Args>(args)...);
	return *this;
}

DEKAF2_NAMESPACE_END

#include <dekaf2/bits/khash.h>

namespace std
{
	DEKAF2_PUBLIC
	std::istream& getline(std::istream& stream, DEKAF2_PREFIX KString& str);
	DEKAF2_PUBLIC
	std::istream& getline(std::istream& stream, DEKAF2_PREFIX KString& str, DEKAF2_PREFIX KString::value_type delimiter);

	/// provide a std::hash for KString
	template<> struct hash<DEKAF2_PREFIX KString>
	{
		// C++20 needs the hasher being marked as is_transparent as well, 
		// not only the comparison functions - only then std::unordered_map
		// permits find(const char*) without creating a temporary string
		using is_transparent = void;

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
DEKAF2_CONSTEXPR_STRING std::size_t DEKAF2_PREFIX KString::Hash() const
//----------------------------------------------------------------------
{
	return std::hash<DEKAF2_PREFIX KString>()(KStringView(*this));
}

//----------------------------------------------------------------------
DEKAF2_CONSTEXPR_STRING std::size_t DEKAF2_PREFIX KString::CaseHash() const
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
