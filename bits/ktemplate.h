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
      std::is_same<char const *, std::decay_t<T>>::value ||
      std::is_same<char *,       std::decay_t<T>>::value
> {};

template<class T>
struct is_wide_c_str
  : std::integral_constant<
      bool,
      std::is_same<wchar_t const *, std::decay_t<T>>::value ||
      std::is_same<wchar_t *,       std::decay_t<T>>::value
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
      std::is_same<const KStringViewZ, std::decay_t<T>>::value ||
      std::is_same<KStringViewZ,       std::decay_t<T>>::value ||
      std::is_same<const KStringView,  std::decay_t<T>>::value ||
      std::is_same<KStringView,        std::decay_t<T>>::value ||
      std::is_same<const KString,      std::decay_t<T>>::value ||
      std::is_same<KString,            std::decay_t<T>>::value ||
      std::is_same<const std::string,  std::decay_t<T>>::value ||
      std::is_same<std::string,        std::decay_t<T>>::value
> {};

template<class T>
struct is_wide_cpp_str
  : std::integral_constant<
      bool,
      std::is_same<const std::wstring, std::decay_t<T>>::value ||
      std::is_same<std::wstring,       std::decay_t<T>>::value
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

