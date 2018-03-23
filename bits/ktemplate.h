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

/// @file ktemplate.h
/// helper templates for template meta programming

#include <functional>
#include <cwctype>
#include <type_traits>
#include "../kstring.h"
#include "../kstringview.h"

namespace dekaf2
{

namespace detail
{

template<class T>
struct is_narrow_c_str
  : std::integral_constant<
      bool,
      std::is_same<char const *, typename std::decay_t<T>::type>::value ||
      std::is_same<char *,       typename std::decay_t<T>::type>::value
> {};

template<class T>
struct is_wide_c_str
  : std::integral_constant<
      bool,
      std::is_same<wchar_t const *, typename std::decay_t<T>::type>::value ||
      std::is_same<wchar_t *,       typename std::decay_t<T>::type>::value
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
      std::is_same<const KStringView, typename std::decay<T>::type>::value ||
      std::is_same<KStringView,       typename std::decay<T>::type>::value ||
      std::is_same<const KString,     typename std::decay<T>::type>::value ||
      std::is_same<KString,           typename std::decay<T>::type>::value ||
      std::is_same<const std::string, typename std::decay<T>::type>::value ||
      std::is_same<std::string,       typename std::decay<T>::type>::value
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

} // of namespace detail

} // of namespace dekaf2

