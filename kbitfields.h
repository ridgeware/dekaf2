/*
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2018, Ridgeware, Inc.
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

/*
 * The KBitfields class was inspired by a presentation of Andrei Alexandrescu at
 * Going Native 2013 (video at
 *   https://channel9.msdn.com/Events/GoingNative/2013/Writing-Quick-Code-in-Cpp-Quickly
 * , code in question starting at minute 17).
 * However, it does not work when coded as presented, particularly when using
 * different sizes for the bitfields, so the below implementation is an own
 * solution that produces correct results.
 * Further benchmarks showed that the built-in bitfields in gcc and clang are
 * today as performant or sometimes even better than the KBitfield class template.
 * This implementation is therefore most useable if a tuple-like access is preferred
 * over access by (variable) names.
 */

#pragma once

/// @file kbitfields.h
/// packing variables with bitfields of configurable bit sizes

#include <cinttypes>
#include <type_traits> // std::enable_if
#include "kcompatibility.h" // enable_if_t < C++14

namespace dekaf2 {
namespace detail {
namespace bitfields {

	template<unsigned... sizes> struct Sum;
	template<unsigned size>
	struct Sum<size> { enum { value = size }; };
	template<unsigned size, unsigned... sizes>
	struct Sum<size, sizes...> { enum { value = size + Sum<sizes...>::value }; };

	static constexpr unsigned alignSum(unsigned sum)
	{
		// keep this a C++11 constexpr (which only allows one return statement)
		return (sum == 0) ? 0
			: (sum <=  8) ? 8
			: (sum <= 16) ? 16
			: (sum <= 32) ? 32
			: (sum <= 64) ? 64
			: sum;
	}

	template<unsigned bits> struct Store;
	template<> struct Store<8>   { using type = uint8_t;  };
	template<> struct Store<16>  { using type = uint16_t; };
	template<> struct Store<32>  { using type = uint32_t; };
	template<> struct Store<64>  { using type = uint64_t; };

} // end of namespace bitfields
} // end of namespace detail

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Accessing bitfields in an integer type with minimal runtime computation.
/// Declare like:
///   KBitfields<3, 5, 2, 6> bf;
/// to create a uint16_t with separately accessible fields with 3, 5, 2, and 6
/// bits of size.
///   bf<1>.get()
/// accesses the second bitfield of size 5 bit. The first bitfield is at the
/// LSB end of the internal storage. Maximum storage capacity is 64 bits, the
/// template rounds the storage type to the next largest fitting integer type.
template<unsigned size, unsigned... sizes>
class KBitfields
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using type = typename detail::bitfields::Store<
	                           detail::bitfields::alignSum(
	                             detail::bitfields::Sum<size, sizes...>::value
	                           )
	                         >::type;

//----------
private:
//----------

	type store;

	//-----------------------------------------------------------------------------
	/// Finally get the bitfield after all offsets have been computed. Specializes
	/// into three access methods by simple if/else.
	template<unsigned pos, unsigned b4, unsigned _size, unsigned... _sizes,
	typename std::enable_if<pos == 0, int>::type = 0>
	type getImpl() const
	//-----------------------------------------------------------------------------
	{
		if (_size == sizeof(store) * 8)
		{
			return store;
		}
		else if (_size == 1)
		{
			return (store & (1ull << b4)) != 0;
		}
		else
		{
			return (store >> b4) & ((1ull << _size) - 1);
		}
	}

	//-----------------------------------------------------------------------------
	/// Recursive calculation of access offsets into the internal storage.
	template<unsigned pos, unsigned b4, unsigned _size, unsigned... _sizes,
	typename std::enable_if<pos != 0, int>::type = 0>
	type getImpl() const
	//-----------------------------------------------------------------------------
	{
		static_assert(pos <= sizeof...(_sizes), "invalid bitfield access");
		return getImpl<pos - 1, b4 + _size, _sizes...>();
	}

	//-----------------------------------------------------------------------------
	/// Finally set the bitfield after all offsets have been computed. Specializes
	/// into two access methods by simple if/else.
	template<unsigned pos, unsigned b4, unsigned _size, unsigned... _sizes,
	typename std::enable_if<pos == 0, int>::type = 0>
	void setImpl(type value)
	//-----------------------------------------------------------------------------
	{
		if (_size == sizeof(store) * 8)
		{
			store = value;
		}
		else
		{
			type mask = ((1ull << _size) - 1) << b4;
			store &= ~mask;
			store |= (value << b4) & mask;
		}
	}

	//-----------------------------------------------------------------------------
	/// Recursive calculation of access offsets into the internal storage.
	template<unsigned pos, unsigned b4, unsigned _size, unsigned... _sizes,
	typename std::enable_if<pos != 0, int>::type = 0>
	void setImpl(type value)
	//-----------------------------------------------------------------------------
	{
		static_assert(pos <= sizeof...(_sizes), "invalid bitfield access");
		return setImpl<pos - 1, b4 + _size, _sizes...>(value);
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	/// Converting / default ctor from base type, sets internal common storage
	KBitfields(type value = 0) : store(value) {}
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set the internal common storage to 'value'
	void setStore(type value) { store = value; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get the internal common storage
	type getStore() const { return store; }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Clear all bitfields
	void clear() { setStore(0); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Get one bitfield
	template<unsigned pos>
	type get() const { return getImpl<pos, 0, size, sizes...>(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Set one bitfield
	template<unsigned pos>
	void set(type value) { setImpl<pos, 0, size, sizes...>(value); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Increase the value of one bitfield by one
	template<unsigned pos>
	void Inc() { set<pos>(get<pos>() + 1); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Decrease the value of one bitfield by one
	template<unsigned pos>
	void Dec() { set<pos>(get<pos>() - 1); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	/// Convert internal common storage into base type
	operator type() const { return getStore(); }
	//-----------------------------------------------------------------------------

}; // KBitfields

} // end of namespace dekaf2

