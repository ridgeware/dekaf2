/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
//
// Copyright (c) 2022, Ridgeware, Inc.
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

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 7
	// need at least gcc 7 to compile
	#undef DEKAF2_KJSON2_IS_DISABLED
	#define DEKAF2_KJSON2_IS_DISABLED 1
#endif

#ifndef DEKAF2_KJSON2_IS_DISABLED
	#if ((__cplusplus <= 201402L) && (defined(__GNUC__) && !defined(__clang__))) \
		|| !__has_include(<nlohmann/json.hpp>)
		// when compiling with GCC this code needs C++17,
		// and in any case we need the nlohmann::json class..
		#define DEKAF2_KJSON2_IS_DISABLED 1
	#else
		#define DEKAF2_KJSON2_IS_DISABLED 0
	#endif
#endif

#if !DEKAF2_KJSON2_IS_DISABLED

#ifdef DEKAF2
	#include "kdefinitions.h"
	#include "kstringview.h"
	#include "kstring.h"
	#include "kjson.h"
#else
	#include <string_view>
	#include <string>
	#include <istream>
	#include <ostream>
	#define JSON_USE_IMPLICIT_CONVERSIONS 0
	#define JSON_HAS_THREE_WAY_COMPARISON 0
	#define JSON_DIAGNOSTICS 0
	#define JSON_USE_GLOBAL_UDLS 0
	#include <nlohmann/json.hpp>
#endif

#ifndef DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
	#define DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE 1
#endif

#ifndef DEKAF2_KJSON_NAMESPACE
	#ifdef DEKAF2_NAMESPACE_NAME
		#define DEKAF2_KJSON_NAMESPACE DEKAF2_NAMESPACE_NAME
	#else
		#define DEKAF2_KJSON_NAMESPACE dekaf2
	#endif
#endif

#ifndef DEKAF2_PUBLIC
	#define DEKAF2_PUBLIC
#endif

#ifndef DEKAF2_TRY
	#define DEKAF2_TRY try
#endif

#ifndef DEKAF2_CATCH
	#define DEKAF2_CATCH(ex) catch(ex)
#endif

#ifndef DEKAF2_LIKELY
	#define DEKAF2_LIKELY
#endif

#ifndef DEKAF2_UNLIKELY
	#define DEKAF2_UNLIKELY
#endif

#ifndef DEKAF2_HAS_INCLUDE
	#define DEKAF2_HAS_INCLUDE(x) __has_include(x)
#endif

#ifndef kDebug
	#define kDebug(iLevel, ...)
#endif

#ifndef kWarning
	#define kWarning(...)
#endif

#ifndef DEKAF2_NODISCARD
	#define DEKAF2_NODISCARD
#endif

#ifndef DEKAF2_NODISCARD_PEDANTIC
	#define DEKAF2_NODISCARD_PEDANTIC
#endif

namespace DEKAF2_KJSON_NAMESPACE {

#ifdef DEKAF2
using BasicJSON = LJSON;
#else
using BasicJSON = nlohmann::json;
#endif





namespace kjson { namespace detail {

// The code in this section of namespace kjson::detail
// (iteration_proxy, iteration_proxy_value, int_to_string)
// is taken as a near copy from nlohmann::json, with modifications
// to support a wrapper class around nlohmann::json::iteration_proxy
//
// The licence is MIT:
//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.11.2
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2022 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

template<typename string_type>
void int_to_string( string_type& target, std::size_t value )
{
	// For ADL
	using std::to_string;
	target = to_string(value);
}

template<>
inline
void int_to_string( BasicJSON::string_t& target, std::size_t value )
{
#ifdef DEKAF2
	target = KString::to_string(value);
#else
	target = std::to_string(value);
#endif
}

template<typename IteratorType> class iteration_proxy_value
{
public:
	using value_t = BasicJSON::value_t;
	using difference_type = std::ptrdiff_t;
	using value_type = iteration_proxy_value;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator_category = std::input_iterator_tag;
	using string_type = typename std::remove_cv< typename std::remove_reference<decltype( std::declval<IteratorType>().key() ) >::type >::type;

private:
	/// the iterator
	IteratorType anchor{};
	/// an index for arrays (used to create key names)
	std::size_t array_index = 0;
	/// last stringified array index
	mutable std::size_t array_index_last = 0;
	/// a string representation of the array index
	mutable string_type array_index_str = "0";
	/// an empty string (to return a reference for primitive values)
	string_type empty_str{};
	/// a copy of the "anchor" type
	value_t object_type = value_t::null;

public:
	explicit iteration_proxy_value() = default;
	explicit iteration_proxy_value(value_t object_type, IteratorType it, std::size_t array_index_ = 0)
	noexcept(std::is_nothrow_move_constructible<IteratorType>::value
			 && std::is_nothrow_default_constructible<string_type>::value)
	: anchor(std::move(it))
	, array_index(array_index_)
	, object_type(object_type)
	{}

	iteration_proxy_value(iteration_proxy_value const&) = default;
	iteration_proxy_value& operator=(iteration_proxy_value const&) = default;
	// older GCCs are a bit fussy and require explicit noexcept specifiers on defaulted functions
	iteration_proxy_value(iteration_proxy_value&&)
	noexcept(std::is_nothrow_move_constructible<IteratorType>::value
			 && std::is_nothrow_move_constructible<string_type>::value) = default;
	iteration_proxy_value& operator=(iteration_proxy_value&&)
	noexcept(std::is_nothrow_move_assignable<IteratorType>::value
			 && std::is_nothrow_move_assignable<string_type>::value) = default;
	~iteration_proxy_value() = default;

	/// dereference operator (needed for range-based for)
	const iteration_proxy_value& operator*() const
	{
		return *this;
	}

	/// increment operator (needed for range-based for)
	iteration_proxy_value& operator++()
	{
		++anchor;
		++array_index;

		return *this;
	}

	iteration_proxy_value operator++(int)& // NOLINT(cert-dcl21-cpp)
	{
		auto tmp = iteration_proxy_value(anchor, array_index);
		++anchor;
		++array_index;
		return tmp;
	}

	/// equality operator (needed for InputIterator)
	bool operator==(const iteration_proxy_value& o) const
	{
		return anchor == o.anchor;
	}

	/// inequality operator (needed for range-based for)
	bool operator!=(const iteration_proxy_value& o) const
	{
		return anchor != o.anchor;
	}

	/// return key of the iterator
	const string_type& key() const
	{
		assert(object_type != value_t::null);

		switch (object_type)
		{
				// use integer array index as key
			case value_t::array:
			{
				if (array_index != array_index_last)
				{
					int_to_string( array_index_str, array_index );
					array_index_last = array_index;
				}
				return array_index_str;
			}

				// use key from the object
			case value_t::object:
				return anchor.key();

				// use an empty key for all primitive types
			case value_t::null:
			case value_t::string:
			case value_t::boolean:
			case value_t::number_integer:
			case value_t::number_unsigned:
			case value_t::number_float:
			case value_t::binary:
			case value_t::discarded:
			default:
				return empty_str;
		}
	}

	/// return value of the iterator
	typename IteratorType::reference value() const
	{
		return anchor.value();
	}
};

/// proxy class for the items() function
template<typename IteratorType> class iteration_proxy
{
private:
	/// the container to iterate
	typename IteratorType::pointer container = nullptr;

public:
	explicit iteration_proxy() = default;

	/// construct iteration proxy from a container
	explicit iteration_proxy(typename IteratorType::reference cont) noexcept
	: container(&cont) {}

	iteration_proxy(iteration_proxy const&) = default;
	iteration_proxy& operator=(iteration_proxy const&) = default;
	iteration_proxy(iteration_proxy&&) noexcept = default;
	iteration_proxy& operator=(iteration_proxy&&) noexcept = default;
	~iteration_proxy() = default;

	/// return iterator begin (needed for range-based for)
	iteration_proxy_value<IteratorType> begin() const noexcept
	{
		return iteration_proxy_value<IteratorType>(container->type(), container->begin());
	}

	/// return iterator end (needed for range-based for)
	iteration_proxy_value<IteratorType> end() const noexcept
	{
		return iteration_proxy_value<IteratorType>(container->type(), container->end());
	}
};


} } // end of namespace kjson::detail, end of code copied from nlohmann::json








// template helpers

namespace kjson { namespace detail {

template<class T>
struct is_char_ptr
: std::integral_constant<
	bool,
	std::is_same<      char*, typename std::decay<T>::type>::value ||
	std::is_same<const char*, typename std::decay<T>::type>::value
> {};

template<class T>
struct is_narrow_cpp_str
  : std::integral_constant<
      bool,
#ifdef DEKAF2
      std::is_same<KString,                typename std::decay<T>::type>::value ||
#endif
      std::is_same<std::string,            typename std::decay<T>::type>::value
> {};

template<class T>
struct is_narrow_string_view
  : std::integral_constant<
      bool,
#ifdef DEKAF2
      std::is_same<KStringViewZ,           typename std::decay<T>::type>::value ||
      std::is_same<KStringView,            typename std::decay<T>::type>::value ||
      std::is_same<sv::string_view,        typename std::decay<T>::type>::value
#else
      std::is_same<std::string_view,       typename std::decay<T>::type>::value
#endif
> {};

} } // end of namespace kjson::detail

#define DEKAF2_FORCE_CHAR_PTR                \
template<typename T,                         \
	typename std::enable_if<                 \
		kjson::detail::is_char_ptr<T>::value \
	, int>::type = 0                         \
>









//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Wrapper class for nlohmann::json that prevents from throwing on unknown keys or unmatching values -
/// instead it changes the json struct on the fly (for a non-const struct) or returns default constructed values
/// (for a const struct). This matches the use of JSON in the internet domain a lot better than the default
/// behavior which meant that you had to test every incoming struct on key validity instead of simply
/// assuming default values for missing keys.
///
/// Also, this class uses string views for input and output where possible.
///
/// A lookup like
/// `std::string str = json["key"]`
/// in the base class creates two strings during execution - the key and the value. The latter is not a
/// problem when assigning, but it is in comparison, e.g. in
/// `if (json["key"] == "value") {}`
/// as no std::strings needed to be allocated normally. This class has special comparison operators
/// to cover the above case without creating strings.
///
/// This class also never creates a string for searching the key.
//
// Implementation hints:
//
// To successfully wrap nlohmann::json as a private base class, we need to
// implement our own iteration_proxy class and its value type iteration_proxy_value.
// The only reason why we have to do so is because the original iteration_proxy_value
// is declared as a friend class to the json base class, and once we declare the
// template for our wrapped json class, the friendship gets lost. Unfortunately
// the access to the "anchor" object type is declared privately in the main iterator
// template of nlohmann::json. Therefore we have to modify the iteration_proxy_value
// class to construct with a dedicated object_type parameter which we store in our own
// accessible variable.
//
class DEKAF2_PUBLIC KJSON2 : private BasicJSON
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	using base            = BasicJSON;

	using reference       = KJSON2&;
	using rvalue_reference= KJSON2&&;
	using const_reference = const KJSON2&;
	using pointer         = KJSON2*;
	using const_pointer   = const KJSON2*;

#ifdef DEKAF2
	using StringT         = KString;
	using StringViewT     = KStringView;
	using IStreamT        = KInStream;
	using OStreamT        = KOutStream;
#else
	using StringT         = std::string;
	using StringViewT     = std::string_view;
	using IStreamT        = std::istream;
	using OStreamT        = std::ostream;
#endif

	class iterator : public base::iterator
	{
	public:
		using iteratorbase = base::iterator; // gcc < 9 complains when using base instead of iteratorbase
		using parent       = KJSON2;
		using reference    = parent::reference;
		using pointer      = parent::pointer;
		// import ctors
		using iteratorbase::iteratorbase;
		// converting ctor
		iterator(iteratorbase it) : iteratorbase(it) {}
		// overwrite operator to wrap reference type
		reference operator*() const { return parent::MakeRef(iteratorbase::operator*());  }
		// overwrite operator to wrap pointer type
		pointer operator->()  const { return parent::MakePtr(iteratorbase::operator->()); }
		// overwrite method to wrap reference type
		reference value()     const { return parent::MakeRef(iteratorbase::value());      }
		// return base reference for this
		const iteratorbase& ToBase() const { return *this; }
		// make the equality comparison available for the wrapped type
		bool operator==(const iterator& other) const { return iteratorbase::operator==(other.ToBase()); }
		// make the unequality comparison available for the wrapped type
		bool operator!=(const iterator& other) const { return iteratorbase::operator!=(other.ToBase()); }
	};

	class const_iterator : public base::const_iterator
	{
	public:
		using iteratorbase    = base::const_iterator; // gcc < 9 complains when using base instead of iteratorbase
		using parent          = KJSON2;
		using const_reference = parent::const_reference;
		using reference       = const_reference;
		using const_pointer   = parent::const_pointer;
		using pointer         = const_pointer;
		// import ctors
		using iteratorbase::iteratorbase;
		// converting ctor
		const_iterator(iteratorbase it) : iteratorbase(it) {}
		// overwrite operator to wrap reference type
		reference operator*() const { return parent::MakeRef(iteratorbase::operator*());  }
		// overwrite operator to wrap pointer type
		pointer operator->()  const { return parent::MakePtr(iteratorbase::operator->()); }
		// overwrite method to wrap reference type
		reference value()     const { return parent::MakeRef(iteratorbase::value());      }
		// return base reference for this
		const iteratorbase& ToBase() const { return *this; }
		// make the equality comparison available for the wrapped type
		bool operator==(const const_iterator& other) const { return iteratorbase::operator==(other.ToBase()); }
		// make the unequality comparison available for the wrapped type
		bool operator!=(const const_iterator& other) const { return iteratorbase::operator!=(other.ToBase()); }
	};

	// imported types

	template<typename iterator_t>
	using iteration_proxy = kjson::detail::iteration_proxy<iterator_t>;

	using base::size_type;
	using base::value_t;
	using base::initializer_list_t;
	using base::exception;
	using base::string_t;
	using base::number_integer_t;
	using base::number_unsigned_t;
	using base::number_float_t;
	using base::boolean_t;

	// we have to pick this one from the private details to enable
	// initialization list construction mixed with KJSON2 values
	using json_ref = nlohmann::detail::json_ref<base>;

					KJSON2() = default;
					KJSON2(base&& other) noexcept : base(std::move(other)) {}

					using base::base;

	// service methods

	/// Parses a string into json, can throw for parsing errors on request
	/// @par sJson the input string with json serializations
	/// @par bThrow if true parser will throw on syntax errors, default is false
	/// @return true on successful parsing, false otherwise
	bool            Parse     (StringViewT sJson, bool bThrow = false);

	/// Parses a stream into json, can throw for parsing errors on request
	/// @par istream the input stream with json serializations
	/// @par bThrow if true parser will throw on syntax errors, default is false
	/// @return true on successful parsing, false otherwise
	bool            Parse     (IStreamT& istream, bool bThrow = false);

	/// Serializes this json element into a string.
	/// @param bPretty uses pretty printing when bPretty is set to true, default is false
	/// @return a string with the serialized json
	DEKAF2_NODISCARD
	StringT         Serialize (bool bPretty = false) const { return Dump(bPretty); }

	/// Serializes this json element into a stream.
	/// @param bPretty uses pretty printing when bPretty is set to true, default is false
	/// @param ostream a stream to receive the serialized json
	void            Serialize (OStreamT& ostream, bool bPretty = false) const;

	/// Serializes this json element into a string.
	/// @param bPretty uses pretty printing when bPretty is set to true, default is false
	/// @return a string with the serialized json
	DEKAF2_NODISCARD
	StringT         Dump      (bool bPretty = false) const { return dump(bPretty ? 1 : -1, '\t'); }

	/// Serializes this json element into a string. Emulates the interface of the base class to select indents etc.
	DEKAF2_NODISCARD
	StringT         dump      (int   iIndent = -1,
							   char  chIndent = ' ',
							   bool  bEnsureASCII = false,
							   const base::error_handler_t error_handler = base::error_handler_t::strict) const;

	// explicit conversions, never throw but return with default values if types do not match,
	// or with best match conversion (e.g. from string to int)

	/// Returns a const reference to this if json is an object. Returns Default parameter if not, which per default references an empty object.
	DEKAF2_NODISCARD_PEDANTIC
	const_reference Object    (const_reference Default = s_oEmpty) const noexcept;

	/// Returns a const reference to this if json is an array. Returns Default parameter if not, which per default references an empty array.
	DEKAF2_NODISCARD_PEDANTIC
	const_reference Array     (const_reference Default = s_oEmpty) const noexcept;

	/// Returns a const string reference to this json element. Will return reference to an empty string if this element is not a string.
	DEKAF2_NODISCARD_PEDANTIC
	const StringT&  ConstString () const noexcept;

	/// Returns a const string reference to this json element. Will return reference to an empty string if this element is not a string.
	DEKAF2_NODISCARD
	const StringT&  String    () const noexcept { return ConstString(); }

	/// Returns a string reference to this json element. If the element is a number, float, or boolean, the value will be converted to string and
	/// stored in place of the previous element. Arrays and objects will be converted to the empty string. To instead return a value for all
	/// json types, use Print() (which is non-destructive).
	DEKAF2_NODISCARD
	StringT&        String    (StringViewT sDefault = StringViewT{}) ;

	/// Returns a string (copy) of this json element. If the element is a number, float, or boolean, the value will be converted to string.
	/// Arrays and objects will be converted to the empty string. To instead return a value for all json types, use Print().
	DEKAF2_NODISCARD
	StringT         CopyString (StringViewT sDefault = StringViewT{}) const { return Print(sDefault, false); }

	/// Returns an unsigned 64 bit integer value for this json element. If the element was a float or string, the value will be converted.
	/// For all other types, the parameter iDefault is returned (which defaults to 0).
	DEKAF2_NODISCARD_PEDANTIC
	uint64_t        UInt64    (uint64_t iDefault = 0)     const noexcept;

	/// Returns a signed 64 bit integer value for this json element. If the element was a float or string, the value will be converted.
	/// For all other types, the parameter iDefault is returned (which defaults to 0).
	DEKAF2_NODISCARD_PEDANTIC
	int64_t         Int64     (int64_t  iDefault = 0)     const noexcept;

	/// Returns a bool value for this json element. If the element was a number, float or string, the value will be converted.
	/// For all other types, the parameter bDefault is returned (which defaults to false).
	DEKAF2_NODISCARD_PEDANTIC
	bool            Bool      (bool     bDefault = false) const noexcept;

	/// Returns a floating point value for this json element. If the element was an integer number or string, the value will be converted.
	/// For all other types, the parameter dDefault is returned (which defaults to 0.0).
	DEKAF2_NODISCARD_PEDANTIC
	double          Float     (double   dDefault = 0.0)   const noexcept;

	/// Returns a string representation for any type of json element, which includes serializations for arrays and objects
	DEKAF2_NODISCARD
	StringT         Print     (StringViewT sDefault = StringViewT{}, bool bSerializeAll = true) const;

	///Returns true if the json element exists, else false. Does not throw.
	DEKAF2_NODISCARD_PEDANTIC
	bool            Exists     () const noexcept { return is_null() == false; }

	// unfortunately, C++ does not allow an implicit conversion to a private base class,
	// therefore we have to declare it as a named method here (and force it manually
	// wherever needed)

	DEKAF2_NODISCARD_PEDANTIC
	constexpr
	const base&    ToBase     () const noexcept { return *this; }
	DEKAF2_NODISCARD_PEDANTIC
	base&          ToBase     ()       noexcept { return *this; }

	// (checked) implicit conversions to value types that do not throw - if the
	// json value does not match it is default constructed
	// - together with the throwless Select() this is all the magic

	operator const StringT&    () const noexcept { return ConstString(); }
#if DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
	operator       StringT&    () &              { return String();      }
	operator       StringT&&   () &&             { return std::move(String()); }
#else
	operator const StringT&    ()       noexcept { return ConstStringRef(); }
#endif
	operator       StringViewT () const noexcept { return ConstString(); }
#if DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
	operator       StringViewT ()                { return String();      }
#endif
#ifdef DEKAF2
	operator       KStringViewZ() const noexcept { return ConstString(); }
#if DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
	operator       KStringViewZ()                { return String();      }
#else
	operator       KStringViewZ()       noexcept { return ConstStringRef(); }
#endif
	operator const std::string&() const noexcept { return ConstString().ToStdString(); }
#if DEKAF2_KJSON_ALLOW_IMPLICIT_CONVERSION_IN_PLACE
	operator       std::string&()  &             { return String().ToStdString();      }
	operator       std::string&&() &&            { return std::move(String().ToStdString()); }
#else
	operator const std::string&()       noexcept { return ConstStringRef().ToStdString(); }
#endif
#endif

	// we have to declare the conversions for fundamental types once explicit (as std::string
	// has an operator=(value_type) that would take any of those), and again with SFINAE
	// to catch all subtypes, matching therefore better or equal to the JSON base class
	explicit operator uint64_t () const noexcept { return UInt64();         }
	explicit operator  int64_t () const noexcept { return  Int64();         }
	explicit operator   double () const noexcept { return  Float();         }
	explicit operator    float () const noexcept { return  Float();         }
	explicit operator     bool () const noexcept { return   Bool();         }

	template < typename ValueType,
		typename std::enable_if <
			std::conjunction <
							   std::is_integral<ValueType>,
							   std::is_unsigned<ValueType>,
				std::negation< std::is_same    <ValueType, std::remove_cv<bool>::type>    >,
				std::negation< std::is_same    <ValueType, typename string_t::value_type> >,
				std::negation< std::is_pointer <ValueType>                                >,
				std::negation< std::is_same    <ValueType, std::nullptr_t>                >
			>::value
		, int >::type = 0>
	operator        ValueType  () const noexcept { return static_cast<ValueType>(UInt64()); }

	template < typename ValueType,
		typename std::enable_if <
			std::conjunction <
							   std::is_integral<ValueType>,
				std::negation< std::is_unsigned<ValueType>                                >,
				std::negation< std::is_same    <ValueType, std::remove_cv<bool>::type>    >,
				std::negation< std::is_same    <ValueType, typename string_t::value_type> >,
				std::negation< std::is_pointer <ValueType>                                >,
				std::negation< std::is_same    <ValueType, std::nullptr_t>                >
			>::value
		, int >::type = 0>
	operator        ValueType  () const noexcept { return static_cast<ValueType>(Int64()); }

	template < typename ValueType,
		typename std::enable_if <
			std::is_same   <ValueType, std::remove_cv<bool>::type>::value
		, int >::type = 0>
	operator        ValueType  () const noexcept { return Bool(); }

	template < typename ValueType,
		typename std::enable_if <
			std::is_floating_point<ValueType>::value
		, int >::type = 0>
	operator        ValueType  () const noexcept { return Float(); }

	constexpr
	operator        value_t    () const noexcept { return type();   }
	operator        json_ref   ()       noexcept { return ToBase(); } // embedded in initializer lists..

	// accessors:

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	ConstElement(StringViewT sElement) const noexcept;

	/// Selects the array index iElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	ConstElement(size_type   iElement) const noexcept;

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	Element    (StringViewT sElement)  const noexcept { return ConstElement(sElement); }

	/// Selects the element in sElement, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Does not throw.
	DEKAF2_NODISCARD
	reference       Element    (StringViewT sElement)        noexcept;

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference Element    (size_type   iElement)  const noexcept { return ConstElement(iElement); }

	/// Selects the array index iSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	DEKAF2_NODISCARD
	reference       Element    (size_type   iElement)        noexcept;

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	ConstSelect(StringViewT sSelector) const noexcept;

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	ConstSelect(size_type   iSelector) const noexcept { return ConstElement(iSelector); }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	Select     (StringViewT sSelector) const noexcept { return ConstSelect(sSelector); }

	/// Selects the key or path in sSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	DEKAF2_NODISCARD
	reference       Select     (StringViewT sSelector)       noexcept;

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference Select     (size_type   iSelector) const noexcept { return ConstSelect(iSelector); }

	/// Selects the array index iSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	DEKAF2_NODISCARD
	reference       Select     (size_type   iSelector)       noexcept { return Element(iSelector);     }

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference operator[] (StringViewT sElement)  const noexcept { return ConstElement(sElement); }

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	reference       operator[] (StringViewT sElement)        noexcept { return Element(sElement);      }

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_FORCE_CHAR_PTR DEKAF2_NODISCARD
	const_reference operator[] (T&& sElement)          const noexcept { return ConstElement(sElement); }

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_FORCE_CHAR_PTR DEKAF2_NODISCARD
	reference       operator[] (T&& sElement)                noexcept { return Element(sElement);      }

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference operator[] (size_type   iElement)  const noexcept { return ConstElement(iElement); }

	/// Selects the array index iSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	DEKAF2_NODISCARD
	reference       operator[] (size_type   iElement)        noexcept { return Element(iElement);      }

	/// Selects the element in sElement, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	operator() (StringViewT sElement)  const noexcept { return ConstElement(sElement); }

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_NODISCARD
	const_reference	operator() (size_type   iElement)  const noexcept { return ConstElement(iElement); }

	// helper methods to ensure that merging objects/arrays/primitives yield
	// expected / palpable results (and do NEVER throw except for malloc),
	// because, yes, we can merge and append, and convert the type if needed)

	reference       Append     (KJSON2 other);

#ifdef DEKAF2
	reference       Merge      (KJSON2 other)       { kjson::Merge (ToBase(), std::move(other.ToBase())); return *this;    }
#endif

	/// Append to array. If current content is not an array it will be converted to one.
	void push_back(KJSON2 json)                     { Append(std::move(json)); }

	/// Merge one key-value pair with existing object. If current content is not an object, it will be converted to an array and the key-value pair be added as a new object to it.
	// needs this SFINAE when compiled with old KJSON as default, to resolve ambiguity with object_t
	template<typename T,
		typename std::enable_if<
			std::is_constructible<object_t, T>::value &&
			!std::is_same<base, typename std::decay<T>::type>::value
		, int>::type = 0
	>
	void push_back(const T& value)                  { try { base::push_back(value); } catch (const exception& e) { Append(value); } }

	/// Merge initializer list with existing object if size == 2 and begin() is string, otherwise add a new object. May fail, but will not throw.
	void push_back(const initializer_list_t value)  { try { base::push_back(value); } catch (const exception& e) {
#ifdef DEKAF2
		kDebug(2, kjson::kStripJSONExceptionMessage(e.what()));
#endif
	} }

	/// Append to array. If current content is not an array it will be converted to one.
	reference operator+=(KJSON2 json)               { push_back(std::move(json)); return *this; }

	/// Merge one key-value pair with existing object. If current content is not an object, it will be converted to
	/// an array and the key-value pair be added as a new object to it.
	// needs this SFINAE when compiled with old KJSON as default, to resolve ambiguity with object_t
	template<typename T,
		typename std::enable_if<
			std::is_constructible<object_t, T>::value &&
			!std::is_same<base, typename std::decay<T>::type>::value
		, int>::type = 0
	>
	reference operator+=(const T& value)           { push_back(value); return *this; }

	/// Merge initializer list with existing object if size == 2 and begin() is string, otherwise try to construct a
	/// new object, convert current content to array, and add the new object to it.
	reference operator+=(const initializer_list_t value)  { push_back(value); return *this; }



	// imported methods from base that do not throw:

	using base::type;
	using base::type_name;
	using base::is_object;
	using base::is_array;
	using base::is_binary;
	using base::is_primitive;
	using base::is_structured;
	using base::is_string;
	using base::is_number;
	using base::is_number_integer;
	using base::is_number_unsigned;
	using base::is_number_float;
	using base::is_boolean;
	using base::is_null;
	using base::empty;
	using base::size;
	using base::max_size;
	using base::count;
	using base::contains;

//	using base::get;     // deprecated
//	using base::get_ref; // deprecated
//	using base::get_ptr; // deprecated
//	using base::get_to;  // deprecated

	// wrapped methods from base to return our wrapper types:

	template<class... Args>
	reference       emplace_back(Args&& ... args)                           { try { return base::emplace_back(std::forward<Args>(args)...); } catch (const exception& e) { return *this; } }

	template<class... Args>
	std::pair<iterator, bool>
	                emplace     (Args&& ... args)                           { try { return base::emplace(std::forward<Args>(args)...); } catch (const exception& e) { return { end(), false }; } }

	const_iterator  erase       (const_iterator it)                         { try { return base::erase(it.ToBase());                   } catch (const exception& e) { return cend(); } }
	iterator        erase       (iterator it)                               { try { return base::erase(it.ToBase());                   } catch (const exception& e) { return  end(); } }
	const_iterator  erase       (const_iterator first, const_iterator last) { try { return base::erase(first.ToBase(), last.ToBase()); } catch (const exception& e) { return cend(); } }
	iterator        erase       (iterator first, iterator last)             { try { return base::erase(first.ToBase(), last.ToBase()); } catch (const exception& e) { return  end(); } }
	size_type       erase       (StringViewT sKey)                          { try { return base::erase(sKey);                          } catch (const exception& e) { return 0;      } }
	void            erase       (const size_type iIndex)                    { try {        base::erase(iIndex);                        } catch (const exception& e) {                } }

	iterator        insert      (const_iterator it, const_reference val)                       { try { return base::insert(it.ToBase(), val.ToBase());                  } catch (const exception& e) { return end(); } }
	iterator        insert      (const_iterator it, rvalue_reference val)                      { try { return base::insert(it.ToBase(), std::move(val.ToBase()));       } catch (const exception& e) { return end(); } }
	iterator        insert      (const_iterator it, size_type count, const_reference val)      { try { return base::insert(it.ToBase(), count, val.ToBase());           } catch (const exception& e) { return end(); } }
	iterator        insert      (const_iterator it, const_iterator first, const_iterator last) { try { return base::insert(it.ToBase(), first.ToBase(), last.ToBase()); } catch (const exception& e) { return end(); } }
	iterator        insert      (const_iterator it, initializer_list_t ilist)                  { try { return base::insert(it.ToBase(), ilist);                         } catch (const exception& e) { return end(); } }
	void            insert      (const_iterator first, const_iterator last)                    { try {        base::insert(first.ToBase(), last.ToBase());              } catch (const exception& e) {               } }

	DEKAF2_NODISCARD
	const_iterator  find        (StringViewT sWhat) const { return base::find(sWhat);      }
	DEKAF2_NODISCARD
	iterator        find        (StringViewT sWhat)       { return base::find(sWhat);      }
	DEKAF2_NODISCARD_PEDANTIC
	const_iterator  begin       ()         const noexcept { return cbegin();               }
	DEKAF2_NODISCARD_PEDANTIC
	const_iterator  end         ()         const noexcept { return cend();                 }
	DEKAF2_NODISCARD_PEDANTIC
	iterator        begin       ()               noexcept { return base::begin();          }
	DEKAF2_NODISCARD_PEDANTIC
	iterator        end         ()               noexcept { return base::end();            }
	DEKAF2_NODISCARD_PEDANTIC
	const_iterator  cbegin      ()         const noexcept { return base::cbegin();         }
	DEKAF2_NODISCARD_PEDANTIC
	const_iterator  cend        ()         const noexcept { return base::cend();           }

	// checked front/back
	DEKAF2_NODISCARD_PEDANTIC
	const_reference	front       ()         const noexcept { try { return MakeRef(base::front()); } catch (const exception& e) { return s_oEmpty; } }
	DEKAF2_NODISCARD_PEDANTIC
	reference       front       ()               noexcept { try { return MakeRef(base::front()); } catch (const exception& e) { return *this;    } }
	DEKAF2_NODISCARD_PEDANTIC
	const_reference	back        ()         const noexcept { try { return MakeRef(base::back ()); } catch (const exception& e) { return s_oEmpty; } }
	DEKAF2_NODISCARD_PEDANTIC
	reference       back        ()               noexcept { try { return MakeRef(base::back ()); } catch (const exception& e) { return *this;    } }

	/// Clears the content - does not reset the content type though, so beware when having an array, clearing, and adding an object..
	/// It will be added to an array, whereas it would have been merged when the type would have been object.
	void            clear       ()               noexcept { return base::clear();          }
	/// returns an iteration_proxy to iterate over key-value pairs of objects
	DEKAF2_NODISCARD
	iteration_proxy<iterator>
	                items       ()               noexcept { return iteration_proxy<iterator>(*this);       }
	/// returns an iteration_proxy to iterate over key-value pairs of objects
	DEKAF2_NODISCARD
	iteration_proxy<const_iterator>
	                items       ()         const noexcept { return iteration_proxy<const_iterator>(*this); }

	/// returns explicitly an array constructed with the values of the initializer list (empty by default)
	DEKAF2_NODISCARD
	static KJSON2   array       (initializer_list_t il = {}) { return base::array  (il);  }
	/// returns explicitly an object constructed with the values of the initializer list (empty by default)
	DEKAF2_NODISCARD
	static KJSON2   object      (initializer_list_t il = {}) { return base::object (il);  }

	void            swap        (reference other) noexcept
	{
		using std::swap;
		swap(this->ToBase(), other.ToBase());
	}

	friend void     swap        (reference left, reference right) noexcept
	{
		using std::swap;
		swap(left.ToBase(), right.ToBase());
	}

	DEKAF2_NODISCARD
	static KJSON2   diff        (const KJSON2& left, const KJSON2& right);

	/// helper function to cast a const reference of type KJSON2 for a const reference of the base json type
	DEKAF2_NODISCARD_PEDANTIC
	static const_reference MakeRef(base::const_reference json) noexcept { return *static_cast<const_pointer>(&json); }
	/// helper function to cast a reference of type KJSON2 for a reference of the base json type
	DEKAF2_NODISCARD_PEDANTIC
	static       reference MakeRef(base::reference json)       noexcept { return *static_cast<pointer>(&json);       }
	/// helper function to cast a const pointer of type KJSON2 for a const pointer of the base json type
	DEKAF2_NODISCARD_PEDANTIC
	static const_pointer   MakePtr(base::const_pointer json)   noexcept { return  static_cast<const_pointer>(json);  }
	/// helper function to cast a pointer of type KJSON2 for a pointer of the base json type
	DEKAF2_NODISCARD_PEDANTIC
	static       pointer   MakePtr(base::pointer json)         noexcept { return  static_cast<pointer>(json);        }

private:

	static constexpr uint16_t iMaxJSONPointerDepth = 1000;

	static const_reference RecurseJSONPointer(const_reference json, StringViewT sSelector, uint16_t iRecursions = 0);
	static       reference RecurseJSONPointer(      reference json, StringViewT sSelector, uint16_t iRecursions = 0);

	static const StringT s_sEmpty;
	static const KJSON2  s_oEmpty;

}; // KJSON2




// ADL conversion functions - these are important to assign KJSON2 in initialization lists
// (which are assembled by the base type), as otherwise always a conversion to string would
// be forced

inline
void to_json  (BasicJSON& j, const KJSON2& j2) { j = j2.ToBase(); }
inline
void from_json(const BasicJSON& j, KJSON2& j2) { j2.ToBase() = j; }





// derived iterator compares between const and non-const

inline bool operator==(const KJSON2::iterator& left, const KJSON2::const_iterator& right)
{
	return left.KJSON2::iterator::iteratorbase::operator==(KJSON2::const_iterator::iteratorbase(right));
}

inline bool operator!=(const KJSON2::iterator& left, const KJSON2::const_iterator& right)
{
	return left.KJSON2::iterator::iteratorbase::operator!=(KJSON2::const_iterator::iteratorbase(right));
}




namespace kjson { namespace detail {

// template helpers

template<class T>
struct is_kjson2
: std::integral_constant<
	bool,
	std::is_same<KJSON2, typename std::decay<T>::type>::value
> {};

template<class T, class U>
struct both_kjson2
: std::integral_constant<
	bool,
	is_kjson2<T>::value && is_kjson2<U>::value
> {};

template<class T, class U>
struct only_first_kjson2
: std::integral_constant<
	bool,
	is_kjson2<T>::value && !is_kjson2<U>::value
> {};

// overloads for check_if_is_default()

template<class U,
	typename std::enable_if<std::is_default_constructible<U>::value && !is_char_ptr<U>::value, int>::type = 0
>
constexpr
bool check_if_is_default(const U& value)
{
	return (U() == value);
}

template<class U,
	typename std::enable_if<!std::is_default_constructible<U>::value, int>::type = 0
>
constexpr
bool check_if_is_default(const U& value)
{
	return false;
}

inline // only constexpr after string is
bool check_if_is_default(const KJSON2::StringT& value)
{
	return value.empty();
}

inline constexpr
bool check_if_is_default(const char* value)
{
	return !value || !*value;
}

// overloads for comparisons

inline
bool compare_eq(const KJSON2& json, const double value)
{
	return json.Float() == value;
}

template<typename U>
typename std::enable_if <
	std::conjunction <
					   std::is_integral<U>,
					   std::is_unsigned<U>,
		std::negation< std::is_same    <U, std::remove_cv<bool>::type>    >,
		std::negation< std::is_same    <U, char>                          >,
		std::negation< std::is_pointer <U>                                >,
		std::negation< std::is_same    <U, std::nullptr_t>                >
	>::value
, bool >::type
compare_eq(const KJSON2& json, const U value)
{
	return json.UInt64() == value;
}

template<typename U>
typename std::enable_if <
	std::conjunction <
					   std::is_integral<U>,
					   std::is_signed  <U>,
		std::negation< std::is_same    <U, std::remove_cv<bool>::type>    >,
		std::negation< std::is_same    <U, char>                          >,
		std::negation< std::is_pointer <U>                                >,
		std::negation< std::is_same    <U, std::nullptr_t>                >
	>::value
, bool >::type
compare_eq(const KJSON2& json, const U value)
{
	return json.Int64() == value;
}

template<typename U>
	typename std::enable_if <
		std::is_same <U, std::remove_cv<bool>::type>::value
	, bool >::type
compare_eq(const KJSON2& json, const U value)
{
	return json.Bool() == value;
}

inline
bool compare_eq(const KJSON2& json, const KJSON2::StringT& value)
{
	return json.String() == value;
}

inline
bool compare_eq(const KJSON2& json, const KJSON2::StringViewT& value)
{
	return json.String() == value;
}

inline
bool compare_eq(const KJSON2& json, const char* value)
{
	return json.String() == value;
}

template<typename U>
typename std::enable_if <
	std::is_integral<U>::value == false &&
	std::is_floating_point<U>::value == false &&
	std::is_same <U, std::remove_cv<bool>::type>::value == false &&
	is_char_ptr<U>::value == false
, bool >::type
compare_eq(const KJSON2& json, const U& value)
{
	return json.ToBase() == value;
}

} } // end of namespace kjson::detail





// comparison

// ==================================

// operator==(KJSON2, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::both_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator==(const T& left, const U& right)
{
	return left.ToBase() == right.ToBase();
}

// operator==(KJSON2, U)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator==(const T& left, const U& right)
{
	if (left.is_null())
	{
		return kjson::detail::check_if_is_default(right);
	}
	return kjson::detail::compare_eq(left, right);
}

// operator==(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator==(const U& left, const T& right)
{
	return right == left;
}

// !=!=!=!=!=!=!=!=!=!=!=!=!=!=!=!=

// operator!=(KJSON2, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::both_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator!=(const T& left, const U& right)
{
	return left.ToBase() != right.ToBase();
}

// operator!=(KJSON2, U)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator!=(const T& left, const U& right)
{
	if (left.is_null())
	{
		return !kjson::detail::check_if_is_default(right);
	}
	return !kjson::detail::compare_eq(left, right);
}

// operator!=(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator!=(const U& left, const T& right)
{
	return right != left;
}

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

// operator<(KJSON2, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::both_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<(const T& left, const U& right)
{
	return left.ToBase() < right.ToBase();
}

// operator<(KJSON2, U)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<(const T& left, const U& right)
{
	return left.ToBase() < right;
}

// operator<(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<(const U& left, const T& right)
{
	return left < right.ToBase();
}

// <=<=<=<=<=<=<=<=<=<=<=<=<=

// operator<=(KJSON2, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::both_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<=(const T& left, const U& right)
{
	return left.ToBase() <= right.ToBase();
}

// operator<=(KJSON2, U)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<=(const T& left, const U& right)
{
	return left.ToBase() <= right;
}

// operator<=(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<=(const U& left, const T& right)
{
	return left <= right.ToBase();
}

// >>>>>>>>>>>>>>>>>>>>>>>>>

// operator>(KJSON2, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::both_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>(const T& left, const U& right)
{
	return left.ToBase() > right.ToBase();
}

// operator>(KJSON2, U)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>(const T& left, const U& right)
{
	return left.ToBase() > right;
}

// operator>(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>(const U& left, const T& right)
{
	return left > right.ToBase();
}

// >=>=>=>=>=>=>=>=>=>=>=>=>=

// operator>=(KJSON2, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::both_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>=(const T& left, const U& right)
{
	return left.ToBase() >= right.ToBase();
}

// operator>=(KJSON2, U)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>=(const T& left, const U& right)
{
	return left.ToBase() >= right;
}

// operator>=(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>=(const U& left, const T& right)
{
	return left >= right.ToBase();
}

// ######################

template<class T,
         typename std::enable_if<
			kjson::detail::is_kjson2<T>::value
		, int>::type = 0
>
DEKAF2_PUBLIC
std::ostream& operator <<(std::ostream& stream, const T& json)
{
	stream << json.Print();
	return stream;
}

template<class T,
         typename std::enable_if<
			kjson::detail::is_kjson2<T>::value
		, int>::type = 0
>
DEKAF2_PUBLIC
std::istream& operator >>(std::istream& stream, T& json)
{
	json.Parse(stream, true);
	return stream;
}












#ifdef DEKAF2

#define DEKAF2_FORCE_KJSON2         \
template<typename T,                \
	typename std::enable_if<        \
		detail::is_kjson2<T>::value \
	, int>::type = 0                \
>

// legacy (checked) operations on the base type, converted to
// the checked type for legacy code - except the parse functions
// do not use these anymore in new code!

namespace kjson {

/// DEPRECATED - use class member
/// Parse a string, throws with KJSON::exception in case of error
/// @param json the json output
/// @param sJSON the input string to parse
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Parse (T& json, KJSON2::StringViewT sJSON)
{
	json.Parse(sJSON, true);
}

/// Parse a string, returns error in sError if any, never throws
/// @param json the json output
/// @param sJSON the input string to parse
/// @param sError the error string
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool Parse (T& json, KJSON2::StringViewT sJSON, KStringRef& sError) noexcept
{
	return Parse(json.ToBase(), sJSON, sError);
}

/// DEPRECATED - use class member
/// Parse a stream, throws with KJSON::exception in case of error
/// @param json the json output
/// @param InStream the input stream to parse
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Parse (T& json, KJSON2::IStreamT& InStream)
{
	json.Parse(InStream, true);
}

/// Parse a stream, returns error in sError if any, never throws
/// @param json the json output
/// @param InStream the input stream to parse
/// @param sError the error string
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool Parse (T& json, KJSON2::IStreamT& InStream, KStringRef& sError) noexcept
{
	return Parse(json.ToBase(), InStream, sError);
}

/// DEPRECATED - use class member
/// returns a string representation for the KJSON object, never throws
/// @param json the json input
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
KJSON2::StringT Print(const T& json) noexcept
{
	return json.Print();
}

/// DEPRECATED - use call operator access
/// Returns a string ref for a key, never throws. Returns empty ref
/// for non-string values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2::StringT& GetStringRef(const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).String();
}

/// DEPRECATED - use call operator access
/// Returns a string value for a key, never throws. Prints non-string
/// values into string representation.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
KJSON2::StringT GetString(const T& json, KJSON2::StringViewT sKey) noexcept
{
	// The old semantics were as follows: if the key does not exist, then return an
	// empty string. If it exists, print anything, even a null element (as NULL).
	// We cannot map that with our new implementation (we do not know if the result
	// of the call operator is null because of a null value, or because of key not found).
	// Hence we fall back to the old implementation
	return GetString(json.ToBase(), sKey);
}

/// DEPRECATED - use call operator access
/// Returns a uint value for a key, never throws. Returns 0 if value was
/// neither an integer nor a string representation of an integer.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
uint64_t GetUInt(const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).UInt64();
}

/// DEPRECATED - use call operator access
/// Returns an int value for a key, never throws. Returns 0 if value was
/// neither an integer nor a string representation of an integer.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
int64_t GetInt(const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).Int64();
}

/// DEPRECATED - use call operator access
/// Returns a bool value for a key, never throws. Returns 0 if value was
/// neither an integer nor a string representation of an integer.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool GetBool(const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).Bool();
}

/// DEPRECATED - use call operator access
/// Returns a const object ref for a key, never throws. Returns empty ref
/// for non-object values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2& GetObjectRef (const T& json, KJSON2::StringViewT sKey) noexcept
{
	// GetObject does not test that the object is actually an object..
	// it just returns any json
	return json(sKey);
}

/// DEPRECATED - use call operator access
/// returns a const object ref for a key, never throws. Returns empty
/// object for non-object values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2& GetObject (const T& json, KJSON2::StringViewT sKey) noexcept
{
	// GetObject does not test that the object is actually an object..
	// it just returns any json
	return json(sKey);
}

/// DEPRECATED - use call operator access
/// returns an array value for a key, never throws. Returns empty
/// array for non-array values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2& GetArray (const T& json, KJSON2::StringViewT sKey) noexcept
{
	// GetArray tests that the object is actually an array..
	return json(sKey).Array();
}

/// DEPRECATED - use member function Exists()
/// returns true if the key exists, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool Exists (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).Exists();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains an object, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsObject (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_object();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains an array, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsArray (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_array();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains a string, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsString (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_string();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains an integer, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsInteger (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_integer();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains a float, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsFloat (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_number_float();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains null, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsNull (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_null();
}

/// DEPRECATED - use call operator access
/// returns true if the key exists and contains a bool, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool IsBoolean (const T& json, KJSON2::StringViewT sKey) noexcept
{
	return json(sKey).is_boolean();
}

/// adds the given integer to the given key, creates key if it does not yet exist
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Increment (T& json, KJSON2::StringViewT sKey, uint64_t iAddMe=1) noexcept
{
	Increment(json.ToBase(), sKey, iAddMe);
}

/// subtracts the given integer from the given key, creates key if it does not yet exist
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Decrement (T& json, KJSON2::StringViewT sKey, uint64_t iSubstractMe=1) noexcept
{
	Decrement(json.ToBase(), sKey, iSubstractMe);
}

/// returns an iterator to the found element if object is a string array or an object and contains the given string, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
KJSON2::const_iterator Find (const T& json, KJSON2::StringViewT sString) noexcept
{
	return Find(json.ToBase(), sString);
}

/// returns true if object is a string array or an object and contains the given string, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool Contains (const T& json, KJSON2::StringViewT sString) noexcept
{
	return Contains(json.ToBase(), sString);
}

/// RecursiveMatchValue (json, m_sSearchX);
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
bool RecursiveMatchValue (const T& json, KJSON2::StringViewT sSearch)
{
	return RecursiveMatchValue(json.ToBase(), sSearch);
}

/// DEPRECATED - use class method
/// merges keys from object2 to object1
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Merge (T& object1, const KJSON2& object2) noexcept
{
	object1.Merge(object2);
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate any type of value inside a JSON structure, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2& Select (const T& json, KJSON2::StringViewT sSelector) noexcept
{
	return json.Select(sSelector);
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate any type of value inside a JSON structure, throws on error
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
KJSON2& Select (T& json, KJSON2::StringViewT sSelector) noexcept
{
	return json.Select(sSelector);
}

/// DEPRECATED - use class method
/// use an integer selector to get an array value, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2& Select (const T& json, std::size_t iSelector) noexcept
{
	return json.Select(iSelector);
}

/// DEPRECATED - use class method
/// use an integer selector to get an array value, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
KJSON2& Select (T& json, std::size_t iSelector) noexcept
{
	return json.Select(iSelector);
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate a string inside a JSON structure, never throws
/// e.g. .data.object.payment.sources[0].creditCard.lastFourDigits
/// or /data/object/payment/sources/0/creditCard/lastFourDigits
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2::StringT& SelectString (const T& json, KJSON2::StringViewT sSelector) noexcept
{
	return json.Select(sSelector).String();
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate an object reference inside a JSON structure, never throws
/// e.g. .data.object.payment.sources[0].creditCard
/// or /data/object/payment/sources/0/creditCard
DEKAF2_FORCE_KJSON2 DEKAF2_NODISCARD DEKAF2_PUBLIC
const KJSON2& SelectObject (const T& json, KJSON2::StringViewT sSelector) noexcept
{
	return json.Select(sSelector).Object();
}

} // end of namespace kjson

// lift a few of the static methods into dekaf2's namespace

using kjson::Parse;
using kjson::Print;
using kjson::GetStringRef;
using kjson::GetString;
using kjson::GetUInt;
using kjson::GetInt;
using kjson::GetBool;
using kjson::GetArray;
using kjson::GetObjectRef;
using kjson::GetObject;
using kjson::Contains;
using kjson::Increment;
using kjson::Decrement;

#undef DEKAF2_FORCE_KJSON2

#else // of #ifdef DEKAF2

// alias KJSON2 into KJSON, as we usually access it by
// the old name
using KJSON = KJSON2;

#endif // of #ifdef DEKAF2

} // end of DEKAF2_KJSON_NAMESPACE

namespace std {

template<>
struct hash<dekaf2::KJSON2>
{
	std::size_t operator()(const dekaf2::KJSON2& json) const
	{
		return hash<dekaf2::BasicJSON>()(json.ToBase());
	}
};

} // end of namespace std

#if DEKAF2_HAS_INCLUDE("kformat.h")

#include "kformat.h"

namespace DEKAF2_FORMAT_NAMESPACE {

template <>
struct formatter<dekaf2::KJSON2> : formatter<string_view>
{
	template <typename FormatContext>
	auto format(const dekaf2::KJSON2& json, FormatContext& ctx) const
	{
		return formatter<string_view>::format(json.Print(), ctx);
	}
};

} // end of DEKAF2_FORMAT_NAMESPACE

#endif // of #if DEKAF2_HAS_INCLUDE("kformat.h")

#undef DEKAF2_FORCE_CHAR_PTR

#endif // of !DEKAF2_KJSON2_IS_DISABLED
