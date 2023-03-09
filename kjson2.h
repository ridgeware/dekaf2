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

#include "bits/ktemplate.h"
#include "kstringview.h"
#include "kstring.h"
#include "kjson.h"

namespace dekaf2 {




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
void int_to_string( KString& target, std::size_t value )
{
	target = KString::to_string(value);
}

template<typename IteratorType> class iteration_proxy_value
{
public:
	using value_t = LJSON::value_t;
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
// Additionally, and very unfortunately, we need to undefine the implicit value cast
// accessor in the json base class, because gcc otherwise fails on resolving overloads
// for our own implicit value casts, because it still sniffs the private base class's
// value casts, even if overwritten by the exact same signature in the derived class,
// and complains them being inaccessible. CLang works fine without this modification.
//
// Also, unfortunately, with gcc it is not possible to implicitly return strings as
// reference types, because this will then not resolve on implicit string constructors,
// like here:
//
// `KString str = json["key"]`
//
// Any other, like `KString str; str = json["key"];` or `KString str(json["key"];`
// would work, but we decided this being too confusing for the user, and hence
// fall back on returning string copies. That then works on all assignments, albeit
// at the cost of an additional string copy (which will of course be optimized out
// for assignments).
// Again, CLang would have no issues here with string ref returns, but in this case
// it is clear that this is stretching C++ conversion rules a bit.
//
// To also allow for string ref returns there is an explicit accessor:
//
// `json["key"].StringRef().`
//
class DEKAF2_PUBLIC KJSON2 : private LJSON
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

public:

	using base            = LJSON;

	using reference       = KJSON2&;
	using const_reference = const KJSON2&;
	using pointer         = KJSON2*;
	using const_pointer   = const KJSON2*;

	class iterator : public LJSON::iterator
	{
	public:
		using base      = LJSON::iterator;
		using parent    = KJSON2;
		using reference = parent::reference;
		using pointer   = parent::pointer;
		// import ctors
		using base::base;
		// converting ctor
		iterator(base it) : base(it) {}
		// overwrite operator to wrap reference type
		reference operator*() const { return parent::MakeRef(base::operator*());  }
		// overwrite operator to wrap pointer type
		pointer operator->()  const { return parent::MakePtr(base::operator->()); }
		// overwrite method to wrap reference type
		reference value()     const { return parent::MakeRef(base::value());      }
		// return base reference for this
		const base& ToBase()  const { return *this; }
		// make the equality comparison available for the wrapped type
		bool operator==(const iterator& other) const { return base::operator==(other.ToBase()); }
		// make the unequality comparison available for the wrapped type
		bool operator!=(const iterator& other) const { return base::operator!=(other.ToBase()); }
	};

	class const_iterator : public LJSON::const_iterator
	{
	public:
		using base            = LJSON::const_iterator;
		using parent          = KJSON2;
		using const_reference = parent::const_reference;
		using reference       = const_reference;
		using const_pointer   = parent::const_pointer;
		using pointer         = const_pointer;
		// import ctors
		using base::base;
		// converting ctor
		const_iterator(base it) : base(it) {}
		// overwrite operator to wrap reference type
		reference operator*() const { return parent::MakeRef(base::operator*());  }
		// overwrite operator to wrap pointer type
		pointer operator->()  const { return parent::MakePtr(base::operator->()); }
		// overwrite method to wrap reference type
		reference value()     const { return parent::MakeRef(base::value());      }
		// return base reference for this
		const base& ToBase()  const { return *this; }
		// make the equality comparison available for the wrapped type
		bool operator==(const const_iterator& other) const { return base::operator==(other.ToBase()); }
		// make the unequality comparison available for the wrapped type
		bool operator!=(const const_iterator& other) const { return base::operator!=(other.ToBase()); }
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
					KJSON2(const KJSON2&) = default;
					KJSON2(KJSON2&&) noexcept = default;
					KJSON2(const LJSON& other) noexcept : base(std::move(other)) {}

					using LJSON::LJSON;

	KJSON2&         operator=(const KJSON2&) = default;
	KJSON2&         operator=(KJSON2&&) = default;

	// service methods

	/// Parses a string into json, can throw for parsing errors on request
	/// @par sJson the input string with json serializations
	/// @par bThrow if true parser will throw on syntax errors, default is false
	/// @return true on successful parsing, false otherwise
	bool            Parse     (KStringView sJson, bool bThrow = false);

	/// Parses a stream into json, can throw for parsing errors on request
	/// @par istream the input stream with json serializations
	/// @par bThrow if true parser will throw on syntax errors, default is false
	/// @return true on successful parsing, false otherwise
	bool            Parse     (KInStream& istream, bool bThrow = false);

	/// Serializes this json element into a string.
	/// @param bPretty uses pretty printing when bPretty is set to true, default is false
	/// @return a string with the serialized json
	KString         Serialize (bool bPretty = false) const { return Dump(bPretty); }

	/// Serializes this json element into a string.
	/// @param bPretty uses pretty printing when bPretty is set to true, default is false
	/// @return a string with the serialized json
	void            Serialize (KOutStream& ostream, bool bPretty = false) const;

	/// Serializes this json element into a string.
	/// @param bPretty uses pretty printing when bPretty is set to true, default is false
	/// @return a string with the serialized json
	KString         Dump      (bool bPretty = false) const { return dump(bPretty ? 1 : -1, '\t'); }

	/// Serializes this json element into a string. Emulates the interface of the base class to select indents etc.
	KString         dump      (int   iIndent = -1,
							   char  chIndent = ' ',
							   bool  bEnsureASCII = false,
							   const base::error_handler_t error_handler = base::error_handler_t::strict) const;

	// explicit conversions, never throw but return with default values if types do not match,
	// or with best match conversion (e.g. from string to int)

	/// Returns a const reference to this if json is an object. Returns Default parameter if not, which per default references an empty object.
	const_reference Object    (const_reference Default = s_oEmpty) const noexcept;

	/// Returns a const reference to this if json is an array. Returns Default parameter if not, which per default references an empty array.
	const_reference Array     (const_reference Default = s_oEmpty) const noexcept;

	/// Returns a const string reference to this json element. Will return reference to an empty string if this element is not a string.
	const KString&  StringRef () const noexcept;

	/// Returns a string reference to this json element. If the element is a number, float, or boolean, the value will be converted to string and
	/// stored in place of the previous element. Arrays and objects will be converted to the empty string. To instead return a value for all
	/// json types, use Print() (which is non-destructive).
	      KString&  StringRef (KStringView sDefault = KStringView{}) ;

	/// Returns a string (copy) of this json element. If the element is a number, float, or boolean, the value will be converted to string.
	/// Arrays and objects will be converted to the empty string. To instead return a value for all json types, use Print().
	      KString   String    (KStringView sDefault = KStringView{}) const;

	/// Returns an unsigned 64 bit integer value for this json element. If the element was a float or string, the value will be converted.
	/// For all other types, the parameter iDefault is returned (which defaults to 0).
	uint64_t        UInt64    (uint64_t iDefault = 0)     const noexcept;

	/// Returns a signed 64 bit integer value for this json element. If the element was a float or string, the value will be converted.
	/// For all other types, the parameter iDefault is returned (which defaults to 0).
	int64_t         Int64     (int64_t  iDefault = 0)     const noexcept;

	/// Returns a bool value for this json element. If the element was a number, float or string, the value will be converted.
	/// For all other types, the parameter bDefault is returned (which defaults to false).
	bool            Bool      (bool     bDefault = false) const noexcept;

	/// Returns a floating point value for this json element. If the element was an integer number or string, the value will be converted.
	/// For all other types, the parameter dDefault is returned (which defaults to 0.0).
	double          Float     (double   dDefault = 0.0)   const noexcept;

	/// Returns a string representation for any type of json element, which includes serializations for arrays and objects
	KString         Print     () const { return kjson::Print(ToBase());     }

	// unfortunately, C++ does not allow an implicit conversion to a private base class,
	// therefore we have to declare it as a named method here (and force it manually
	// wherever needed)

	const base&    ToBase     () const noexcept { return *this; }
	base&          ToBase     ()       noexcept { return *this; }

	// (checked) implicit conversions to value types that do not throw - if the
	// json value does not match it is default constructed
	// - together with the throwless Select() this is all the magic

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

	template < typename ValueType,
		typename std::enable_if <
			detail::is_narrow_cpp_str<ValueType>::value &&
			!detail::is_string_view<ValueType>::value
		, int >::type = 0>
	operator        ValueType  () const          { return String(); }

	template < typename ValueType,
		typename std::enable_if <
			detail::is_string_view<ValueType>::value
		, int >::type = 0>
	operator        ValueType  () const noexcept { return StringRef(); }

	constexpr
	operator        value_t    () const noexcept { return type();   }
	operator        json_ref   ()       noexcept { return ToBase(); } // embedded in initializer lists..

	// accessors that permit to return selectors with any of pointer or single element syntax

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference	SelectConst(KStringView sSelector) const noexcept { return MakeRef(kjson::Select(ToBase(), sSelector)); }

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference	SelectConst(size_type   iSelector) const noexcept { return MakeRef(kjson::Select(ToBase(), iSelector)); }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference	Select     (KStringView sSelector) const noexcept { return SelectConst(sSelector); }

	/// Selects the key or path in sSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	reference       Select     (KStringView sSelector)       noexcept { return MakeRef(kjson::Select(ToBase(), sSelector)); }

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference Select     (size_type   iSelector) const noexcept { return SelectConst(iSelector); }

	/// Selects the array index iSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	reference       Select     (size_type   iSelector)       noexcept { return MakeRef(kjson::Select(ToBase(), iSelector)); }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference operator[] (KStringView sSelector) const noexcept { return SelectConst(sSelector); }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	reference       operator[] (KStringView sSelector)       noexcept { return Select(sSelector);      }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_FORCE_CHAR_PTR
	const_reference operator[] (T&& sSelector)         const noexcept { return SelectConst(sSelector); }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	DEKAF2_FORCE_CHAR_PTR
	reference       operator[] (T&& sSelector)               noexcept { return Select(sSelector);      }

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference operator[] (size_type   iSelector) const noexcept { return SelectConst(iSelector); }

	/// Selects the array index iSelector, and returns a reference to the found json element. In case of unknown element inserts a new element, and returns a reference to this. Expands arrays if needed. Does not throw.
	reference       operator[] (size_type   iSelector)       noexcept { return Select(iSelector);      }

	/// Selects the key or path in sSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference	operator() (KStringView sSelector) const noexcept { return SelectConst(sSelector); }

	/// Selects the array index iSelector, and returns a const reference to the found json element. In case of unknown element returns reference to an empty json element. Does not throw.
	const_reference	operator() (size_type   iSelector) const noexcept { return SelectConst(iSelector); }

	// helper methods to ensure that merging objects/arrays/primitives yield
	// expected / palpable results (and do NEVER throw except for malloc),
	// because, yes, we can merge and append, and convert the type if needed)

	reference       Append     (KJSON2 other)       { kjson::Append(ToBase(), std::move(other.ToBase())); return *this;    }
	reference       Merge      (KJSON2 other)       { kjson::Merge (ToBase(), std::move(other.ToBase())); return *this;    }

	/// Append to array. If current content is not an array it will be converted to one.
	void push_back(KJSON2 json)                     { Append(std::move(json)); }

	/// Merge one key-value pair with existing object. If current content is not an object, it will be converted to an array and the key-value pair be added as a new object to it.
	// needs this SFINAE when compiled with old KJSON as default, to resolve ambiguity with object_t
	template<typename T,
		typename std::enable_if<
			std::is_constructible<object_t, T>::value &&
			!std::is_same<LJSON, typename std::decay<T>::type>::value
		, int>::type = 0
	>
	void push_back(const T& value)                  { try { base::push_back(value); } catch (const exception& e) { Append(value); } }

	/// Merge initializer list with existing object if size == 2 and begin() is string, otherwise add a new object. May fail, but will not throw.
	void push_back(const initializer_list_t value)  { try { base::push_back(value); } catch (const exception& e) { kDebug(1, kjson::kStripJSONExceptionMessage(e.what())); } }

	/// Append to array. If current content is not an array it will be converted to one.
	reference operator+=(KJSON2 json)               { push_back(std::move(json)); return *this; }

	/// Merge one key-value pair with existing object. If current content is not an object, it will be converted to
	/// an array and the key-value pair be added as a new object to it.
	// needs this SFINAE when compiled with old KJSON as default, to resolve ambiguity with object_t
	template<typename T,
		typename std::enable_if<
			std::is_constructible<object_t, T>::value &&
			!std::is_same<LJSON, typename std::decay<T>::type>::value
		, int>::type = 0
	>
	reference operator+=(const T& value)           { push_back(value); return *this; }

	/// Merge initializer list with existing object if size == 2 and begin() is string, otherwise try to construct a
	/// new object, convert current content to array, and add the new object to it.
	reference operator+=(const initializer_list_t value)  { push_back(value); return *this; }



	// imported methods from base which are exception safe

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

	// imported methods from base which are not exception safe

	using base::emplace; // add checks
	using base::erase;   // add checks
//  using base::insert;  // add checks
	using base::get;     // deprecated
	using base::get_ref; // deprecated
	using base::get_ptr; // deprecated
	using base::get_to;  // deprecated

	// wrapped methods from base to return our wrapper types

	const_iterator  find        (KStringView sWhat) const { return base::find(sWhat);      }
	iterator        find        (KStringView sWhat)       { return base::find(sWhat);      }
	const_iterator  begin       ()         const noexcept { return cbegin();               }
	const_iterator  end         ()         const noexcept { return cend();                 }
	iterator        begin       ()               noexcept { return base::begin();          }
	iterator        end         ()               noexcept { return base::end();            }
	const_iterator  cbegin      ()         const noexcept { return base::cbegin();         }
	const_iterator  cend        ()         const noexcept { return base::cend();           }
	const_reference	front       ()         const noexcept { return MakeRef(base::front()); }
	reference       front       ()               noexcept { return MakeRef(base::front()); }
	const_reference	back        ()         const noexcept { return MakeRef(base::back());  }
	reference       back        ()               noexcept { return MakeRef(base::back());  }
	/// Clears the content - does not reset the content type though, so beware when having an array, clearing, and adding an object..
	/// It will be added to an array, whereas it would have been merged when the type would have been object.
	void            clear       ()               noexcept { return base::clear();          }
	/// returns an iteration_proxy to iterate over key-value pairs of objects
	iteration_proxy<iterator>
	                items       ()               noexcept { return iteration_proxy<iterator>(*this);       }
	/// returns an iteration_proxy to iterate over key-value pairs of objects
	iteration_proxy<const_iterator>
	                items       ()         const noexcept { return iteration_proxy<const_iterator>(*this); }

	/// returns explicitly an array constructed with the values of the initializer list (empty by default)
	static KJSON2   array       (initializer_list_t il = {}) { return base::array  (il);  }
	/// returns explicitly an object constructed with the values of the initializer list (empty by default)
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

	static KJSON2   diff        (const KJSON2& left, const KJSON2& right);

	/// helper function to cast a const reference of type KJSON2 for a const reference of the base json type (LJSON)
	static const_reference MakeRef(base::const_reference json) noexcept { return *static_cast<const_pointer>(&json); }
	/// helper function to cast a reference of type KJSON2 for a reference of the base json type (LJSON)
	static       reference MakeRef(base::reference json)       noexcept { return *static_cast<pointer>(&json);       }
	/// helper function to cast a const pointer of type KJSON2 for a const pointer of the base json type (LJSON)
	static const_pointer   MakePtr(base::const_pointer json)   noexcept { return  static_cast<const_pointer>(json);  }
	/// helper function to cast a pointer of type KJSON2 for a pointer of the base json type (LJSON)
	static       pointer   MakePtr(base::pointer json)         noexcept { return  static_cast<pointer>(json);        }

private:

	static const KString s_sEmpty;
	static const KJSON2  s_oEmpty;

}; // KJSON2




// shorthand to create a KJSON2 ref from a LJSON ref

inline const KJSON2& KJSON2Ref(const LJSON& json) { return KJSON2::MakeRef(json); }
inline       KJSON2& KJSON2Ref(      LJSON& json) { return KJSON2::MakeRef(json); }



// ADL conversion functions - these are important to assign KJSON2 in initialization lists
// (which are assembled by the base type), as otherwise always a conversion to string would
// be forced

inline
void to_json  (LJSON& j, const KJSON2& j2) { j = j2.ToBase(); }
inline
void from_json(const LJSON& j, KJSON2& j2) { j2.ToBase() = j; }





// derived iterator compares between const and non-const

inline bool operator==(const KJSON2::iterator& left, const KJSON2::const_iterator& right)
{
	return left.KJSON2::iterator::base::operator==(KJSON2::const_iterator::base(right));
}

inline bool operator!=(const KJSON2::iterator& left, const KJSON2::const_iterator& right)
{
	return left.KJSON2::iterator::base::operator!=(KJSON2::const_iterator::base(right));
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
bool check_if_is_default(const KString& value)
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
bool compare_eq(const KJSON2& json, const KString& value)
{
	return json.StringRef() == value;
}

inline
bool compare_eq(const KJSON2& json, const KStringView& value)
{
	return json.StringRef() == value;
}

inline
bool compare_eq(const KJSON2& json, const char* value)
{
	return json.StringRef() == value;
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
bool operator==(const T& left, U&& right)
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
bool operator==(U&& left, const T& right)
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
bool operator!=(const T& left, U&& right)
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
bool operator!=(U&& left, const T& right)
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
bool operator<(T&& left, U&& right)
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
bool operator<(T&& left, U&& right)
{
	return left.ToBase() < std::forward<U>(right);
}

// operator<(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<(U&& left, const T& right)
{
	return std::forward<U>(left) < right.ToBase();
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
bool operator<=(const T& left, U&& right)
{
	return left.ToBase() <= std::forward<U>(right);
}

// operator<=(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator<=(U&& left, const T& right)
{
	return std::forward<U>(left) <= right.ToBase();
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
bool operator>(const T& left, U&& right)
{
	return left.ToBase() > std::forward<U>(right);
}

// operator>(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>(U&& left, const T& right)
{
	return std::forward<U>(left) > right.ToBase();
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
bool operator>=(const T& left, U&& right)
{
	return left.ToBase() >= std::forward<U>(right);
}

// operator>=(U, KJSON2)
template<class T, class U,
	typename std::enable_if<
		kjson::detail::only_first_kjson2<T, U>::value
	, int>::type = 0
>
DEKAF2_PUBLIC
bool operator>=(U&& left, const T& right)
{
	return std::forward<U>(left) >= right.ToBase();
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

/// Parse a string, throws with KJSON::exception in case of error
/// @param json the json output
/// @param sJSON the input string to parse
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Parse (T& json, KStringView sJSON)
{
	Parse(json.ToBase(), sJSON);
}

/// Parse a string, returns error in sError if any, never throws
/// @param json the json output
/// @param sJSON the input string to parse
/// @param sError the error string
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool Parse (T& json, KStringView sJSON, KStringRef& sError) noexcept
{
	return Parse(json.ToBase(), sJSON, sError);
}

/// Parse a stream, throws with KJSON::exception in case of error
/// @param json the json output
/// @param InStream the input stream to parse
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Parse (T& json, KInStream& InStream)
{
	Parse(json.ToBase(), InStream);
}

/// Parse a stream, returns error in sError if any, never throws
/// @param json the json output
/// @param InStream the input stream to parse
/// @param sError the error string
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool Parse (T& json, KInStream& InStream, KStringRef& sError) noexcept
{
	return Parse(json.ToBase(), InStream, sError);
}

/// DEPRECATED - use class member
/// returns a string representation for the KJSON object, never throws
/// @param json the json input
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
KString Print(const T& json) noexcept
{
	return json.Print();
}

/// DEPRECATED - use Select or call access
/// Returns a string ref for a key, never throws. Returns empty ref
/// for non-string values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KString& GetStringRef(const T& json, KStringView sKey) noexcept
{
	return json(sKey).StringRef();
}

/// DEPRECATED - use Select or call access
/// Returns a string value for a key, never throws. Prints non-string
/// values into string representation.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
KString GetString(const T& json, KStringView sKey) noexcept
{
	return GetString(json.ToBase(), sKey);
}

/// DEPRECATED - use Select or call access
/// Returns a uint value for a key, never throws. Returns 0 if value was
/// neither an integer nor a string representation of an integer.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
uint64_t GetUInt(const T& json, KStringView sKey) noexcept
{
	return json(sKey).UInt64();
}

/// DEPRECATED - use Select or call access
/// Returns an int value for a key, never throws. Returns 0 if value was
/// neither an integer nor a string representation of an integer.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
int64_t GetInt(const T& json, KStringView sKey) noexcept
{
	return json(sKey).Int64();
}

/// DEPRECATED - use Select or call access
/// Returns a bool value for a key, never throws. Returns 0 if value was
/// neither an integer nor a string representation of an integer.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool GetBool(const T& json, KStringView sKey) noexcept
{
	return json(sKey).Bool();
}

/// DEPRECATED - use Select or call access
/// Returns a const object ref for a key, never throws. Returns empty ref
/// for non-object values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KJSON2& GetObjectRef (const T& json, KStringView sKey) noexcept
{
	// GetObject does not test that the object is actually an object..
	// it just returns any json
	return json(sKey);
}

/// DEPRECATED - use Select or call access
/// returns a const object ref for a key, never throws. Returns empty
/// object for non-object values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KJSON2& GetObject (const T& json, KStringView sKey) noexcept
{
	// GetObject does not test that the object is actually an object..
	// it just returns any json
	return json(sKey);
}

/// DEPRECATED - use Select or call access
/// returns an array value for a key, never throws. Returns empty
/// array for non-array values.
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KJSON2& GetArray (const T& json, KStringView sKey) noexcept
{
	// GetArray tests that the object is actually an array..
	return json(sKey).Array();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool Exists (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_null() == false;
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains an object, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsObject (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_object();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains an array, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsArray (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_array();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains a string, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsString (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_string();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains an integer, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsInteger (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_integer();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains a float, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsFloat (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_number_float();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains null, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsNull (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_null();
}

/// DEPRECATED - use Select or call access
/// returns true if the key exists and contains a bool, never throws
/// @param json the json input
/// @param sKey the key to search for
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool IsBoolean (const T& json, KStringView sKey) noexcept
{
	return json(sKey).is_boolean();
}

/// adds the given integer to the given key, creates key if it does not yet exist
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Increment (T& json, KStringView sKey, uint64_t iAddMe=1) noexcept
{
	Increment(json.ToBase(), sKey, iAddMe);
}

/// subtracts the given integer from the given key, creates key if it does not yet exist
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
void Decrement (T& json, KStringView sKey, uint64_t iSubstractMe=1) noexcept
{
	Decrement(json.ToBase(), sKey, iSubstractMe);
}

/// returns an iterator to the found element if object is a string array or an object and contains the given string, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
KJSON2::const_iterator Find (const T& json, KStringView sString) noexcept
{
	return Find(json.ToBase(), sString);
}

/// returns true if object is a string array or an object and contains the given string, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool Contains (const T& json, KStringView sString) noexcept
{
	return Contains(json.ToBase(), sString);
}

/// RecursiveMatchValue (json, m_sSearchX);
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
bool RecursiveMatchValue (const T& json, KStringView sSearch)
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
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KJSON2& Select (const T& json, KStringView sSelector) noexcept
{
	return json(sSelector);
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate any type of value inside a JSON structure, throws on error
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
KJSON2& Select (T& json, KStringView sSelector) noexcept
{
	return json(sSelector);
}

/// DEPRECATED - use class method
/// use an integer selector to get an array value, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KJSON2& Select (const T& json, std::size_t iSelector) noexcept
{
	return json(iSelector);
}

/// DEPRECATED - use class method
/// use an integer selector to get an array value, never throws
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
KJSON2& Select (T& json, std::size_t iSelector) noexcept
{
	return json(iSelector);
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate a string inside a JSON structure, never throws
/// e.g. .data.object.payment.sources[0].creditCard.lastFourDigits
/// or /data/object/payment/sources/0/creditCard/lastFourDigits
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KString& SelectString (const T& json, KStringView sSelector) noexcept
{
	return json(sSelector).StringRef();
}

/// DEPRECATED - use class method
/// use a path-style selector to isolate an object reference inside a JSON structure, never throws
/// e.g. .data.object.payment.sources[0].creditCard
/// or /data/object/payment/sources/0/creditCard
DEKAF2_FORCE_KJSON2 DEKAF2_PUBLIC
const KJSON2& SelectObject (const T& json, KStringView sSelector) noexcept
{
	return json(sSelector).Object();
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

} // end of namespace dekaf2

namespace std {

template<>
struct hash<dekaf2::KJSON2>
{
	std::size_t operator()(const dekaf2::KJSON2& json) const
	{
		return hash<dekaf2::LJSON>()(json.ToBase());
	}
};

} // end of namespace std

#undef DEKAF2_FORCE_KJSON2
#undef DEKAF2_FORCE_CHAR_PTR
