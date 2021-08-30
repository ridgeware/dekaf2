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

/// @file ktemplate.h
/// helper templates for template meta programming

#include <functional>
#include <cwctype>
#include <type_traits>
#include "kcppcompat.h"

// we do general algorithms before including kstring.h / kstringview.h below

namespace dekaf2
{

namespace detail
{

template <template <typename> class F>
struct conversion_tester
{
    template <typename T>
    conversion_tester (const F<T> &);
};

template <class From, template <typename> class To>
struct is_instance_of
{
    static const bool value = std::is_convertible<From, conversion_tester<To>>::value;
};

// primary template handles types that have no key_type member:
template< class, class = std::void_t<> >
struct has_key_type : std::false_type { };

// specialization recognizes types that do have a key_type member:
template< class T >
struct has_key_type<T, std::void_t<typename T::key_type>> : std::true_type { };

// primary template handles types that have no mapped_type member:
template< class, class = std::void_t<> >
struct has_mapped_type : std::false_type { };

// specialization recognizes types that do have a mapped_type member:
template< class T >
struct has_mapped_type<T, std::void_t<typename T::mapped_type>> : std::true_type { };

template<class T>
struct is_map_type
  : std::integral_constant<
	  bool,
	  has_key_type<T>::value &&
	  has_mapped_type<T>::value
> {};

template<class T>
struct is_set_type
  : std::integral_constant<
	  bool,
	  has_key_type<T>::value &&
	  !has_mapped_type<T>::value
> {};

template <typename Container>
struct is_std_array : std::false_type { };

template <typename T, std::size_t S>
struct is_std_array<std::array<T, S>> : std::true_type { };

template<typename T, typename Index, typename = void>
struct has_subscript_operator : std::false_type { };

template<typename T, typename Index>
struct has_subscript_operator<T, Index, std::void_t<
	decltype(std::declval<T>()[std::declval<Index>()])
>> : std::true_type { };

// returns has_size<T>::value == true when type has a size() member function
template <typename T>
class has_size
{
private:
	typedef char Yes;
	typedef char No[2];

	template<typename C> static auto Test(void*)
	-> decltype(size_t{std::declval<C const>().size()}, Yes{});

	template<typename> static No& Test(...);

public:
	static constexpr bool const value = sizeof(Test<T>(0)) == sizeof(Yes);
};

template <typename>
struct is_chrono_duration : std::false_type {};

template <typename R, typename P>
struct is_chrono_duration<std::chrono::duration<R, P>> : std::true_type {};

} // of namespace detail

} // of namespace dekaf2

// now include kstring / view.h for the string tests

//#include "../kstring.h"
//#include "../kstringview.h"
//#include "kstringviewz.h"

namespace dekaf2
{

class KString;
class KStringView;
class KStringViewZ;

namespace detail
{

template<class T>
struct is_narrow_c_str
  : std::integral_constant<
      bool,
      std::is_same<char const *, typename std::decay<T>::type>::value ||
      std::is_same<char *,       typename std::decay<T>::type>::value
> {};

template<class T>
struct is_wide_c_str
  : std::integral_constant<
      bool,
      std::is_same<wchar_t const *, typename std::decay<T>::type>::value ||
      std::is_same<wchar_t *,       typename std::decay<T>::type>::value
> {};

template<class T>
struct is_c_str
  : std::integral_constant<
      bool,
        is_narrow_c_str<T>::value ||
        is_wide_c_str<T>::value
> {};

template<class T>
struct is_narrow_cpp_str
  : std::integral_constant<
      bool,
      std::is_same<const KStringViewZ, typename std::decay<T>::type>::value ||
      std::is_same<KStringViewZ,       typename std::decay<T>::type>::value ||
      std::is_same<const KStringView,  typename std::decay<T>::type>::value ||
      std::is_same<KStringView,        typename std::decay<T>::type>::value ||
      std::is_same<const KString,      typename std::decay<T>::type>::value ||
      std::is_same<KString,            typename std::decay<T>::type>::value ||
      std::is_same<const std::string,  typename std::decay<T>::type>::value ||
      std::is_same<std::string,        typename std::decay<T>::type>::value
> {};

template<class T>
struct is_wide_cpp_str
  : std::integral_constant<
      bool,
      std::is_same<const std::wstring, typename std::decay<T>::type>::value ||
      std::is_same<std::wstring,       typename std::decay<T>::type>::value
> {};

template<class T>
struct is_cpp_str
  : std::integral_constant<
      bool,
      is_narrow_cpp_str<T>::value ||
      is_wide_cpp_str<T>::value
> {};

template<class T>
struct is_narrow_str
  : std::integral_constant<
      bool,
      is_narrow_cpp_str<T>::value ||
      is_narrow_c_str<T>::value
> {};

template<class T>
struct is_wide_str
  : std::integral_constant<
      bool,
      is_wide_cpp_str<T>::value ||
      is_wide_c_str<T>::value
> {};

template<class T>
struct is_str
  : std::integral_constant<
      bool,
      is_cpp_str<T>::value ||
      is_c_str<T>::value
> {};

} // of namespace detail

} // of namespace dekaf2

