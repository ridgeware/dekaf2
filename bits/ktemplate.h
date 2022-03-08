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

#include "kcppcompat.h"
#include <functional>
#include <cwctype>
#include <type_traits>

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

// primary template handles types that have no json_pointer member type:
template< class, class = std::void_t<> >
struct is_json_type : std::false_type { };

// specialization recognizes types that do have a json_pointer member type and
// therefore most likely are our JSON type:
template< class T >
struct is_json_type<T, std::void_t<typename std::decay<T>::type::json_pointer>> : std::true_type { };

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

#define DEKAF2_HAS_MEMBER_FUNCTION(function, has_name)                           \
	template <typename T>                                                        \
	class has_name                                                               \
	{                                                                            \
	private:                                                                     \
		typedef char Yes;                                                        \
		typedef char No[2];                                                      \
                                                                                 \
		template<typename C> static auto Test(void*)                             \
		-> decltype(size_t{std::declval<C const>().function()}, Yes{});          \
                                                                                 \
		template<typename> static No& Test(...);                                 \
                                                                                 \
	public:                                                                      \
		static constexpr bool const value = sizeof(Test<T>(0)) == sizeof(Yes);   \
	}

DEKAF2_HAS_MEMBER_FUNCTION(size,      has_size     );
DEKAF2_HAS_MEMBER_FUNCTION(Parse,     has_Parse    );
DEKAF2_HAS_MEMBER_FUNCTION(Serialize, has_Serialize);

template <typename>
struct is_chrono_duration : std::false_type {};

template <typename R, typename P>
struct is_chrono_duration<std::chrono::duration<R, P>> : std::true_type {};

template<class T>
struct is_pod
  : std::integral_constant<
	  bool,
	  std::is_standard_layout<T>::value &&
	  std::is_trivial<T>::value
> {};


} // of namespace detail

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
      std::is_same<const KStringViewZ,     typename std::decay<T>::type>::value ||
      std::is_same<KStringViewZ,           typename std::decay<T>::type>::value ||
      std::is_same<const KStringView,      typename std::decay<T>::type>::value ||
      std::is_same<KStringView,            typename std::decay<T>::type>::value ||
      std::is_same<const KString,          typename std::decay<T>::type>::value ||
      std::is_same<KString,                typename std::decay<T>::type>::value ||
#if defined(DEKAF2_HAS_CPP_17) && (!defined(DEKAF2_IS_GCC) || DEKAF2_GCC_VERSION >= 70000)
      std::is_same<const std::string_view, typename std::decay<T>::type>::value ||
      std::is_same<std::string_view,       typename std::decay<T>::type>::value ||
#endif
      std::is_same<const std::string,      typename std::decay<T>::type>::value ||
      std::is_same<std::string,            typename std::decay<T>::type>::value
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

template<class T, bool bAllowCharPointer = false>
struct is_kstringview_assignable
: std::integral_constant<
	bool,
	!std::is_same<char, typename std::decay<T>::type>::value &&
	(bAllowCharPointer || !is_narrow_c_str<T>::value) &&
	std::is_assignable<KStringView, T>::value
> {};

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to wrap one object reference and access it through a proxy object
template<class T>
class ReferenceProxy
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ReferenceProxy(T& proxied) : m_proxied(proxied) {};

	/// get reference on object
	T& get()
	{
		return m_proxied;
	}

	/// get pointer on object
	T* operator->()
	{
		return &get();
	}

	/// get reference on object
	T& operator*()
	{
		return get();
	}

	/// get reference on object
	operator T&()
	{
		return get();
	}

	/// helper for subscript access - acts as a proxy for the real object
	template<typename KeyType, typename Map = T,
			 typename std::enable_if<detail::is_map_type<Map>::value == true, int>::type = 0>
	typename Map::mapped_type& operator[](KeyType&& Key)
	{
		return get().operator[](std::forward<KeyType>(Key));
	}

	template<typename KeyType, typename Array = T,
			 typename std::enable_if<detail::is_std_array<Array>::value == true, int>::type = 0>
	typename Array::value_type& operator[](KeyType&& Key)
	{
		return get().operator[](std::forward<KeyType>(Key));
	}

//----------
private:
//----------

	T& m_proxied;

}; // ReferenceProxy

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// helper class to const wrap one object reference and access it through a proxy object
template<class T>
class ConstReferenceProxy
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	ConstReferenceProxy(const T& proxied) : m_proxied(proxied) {};

	/// get const reference on object
	const T& get() const
	{
		return m_proxied;
	}

	/// get const pointer on object
	const T* operator->() const
	{
		return &get();
	}

	/// get const reference on object
	const T& operator*() const
	{
		return get();
	}

	/// get const reference on object
	operator const T&() const
	{
		return get();
	}

	/// helper for subscript access - acts as a proxy for the real object
	template<typename KeyType, typename Map = T,
			 typename std::enable_if<detail::is_map_type<Map>::value == true, int>::type = 0>
	const typename Map::mapped_type& operator[](KeyType&& Key)
	{
		return get().at(std::forward<KeyType>(Key));
	}

	template<typename KeyType, typename Array = T,
			 typename std::enable_if<detail::is_std_array<Array>::value == true, int>::type = 0>
	const typename Array::value_type& operator[](KeyType&& Key)
	{
		return get().at(std::forward<KeyType>(Key));
	}

//----------
private:
//----------

	const T& m_proxied;

}; // ConstReferenceProxy

} // of namespace detail

} // of namespace dekaf2

