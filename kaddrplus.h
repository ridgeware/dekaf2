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

#pragma once

/// @file kaddrplus.h
/// using the MSB of a pointer for other purposes

#include "bits/kcppcompat.h"
#include "kbitfields.h"

namespace dekaf2 {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Experimental type to use the upper bits of an address for other purposes.
/// On a 64 bit system you should best not use more than 22 bits.. anyway, the
/// type is experimental. Do not shoot yourself in the foot.
template<typename AddressOf, unsigned... sizes>
class KAddrPlus
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

	static_assert(sizeof(void*)*8 - detail::bitfields::Sum<sizes...>::value > sizeof(void*)*8/2, "too many bits used");

//----------
private:
//----------

	using Storage = KBitfields<sizeof(void*)*8 - detail::bitfields::Sum<sizes...>::value, sizes...>;

	Storage m_Store;

	//-----------------------------------------------------------------------------
	AddressOf* getAddress() const
	//-----------------------------------------------------------------------------
	{
		return reinterpret_cast<AddressOf*>(m_Store.template get<0>());
	}

	//-----------------------------------------------------------------------------
	void setAddress(AddressOf* address)
	//-----------------------------------------------------------------------------
	{
		m_Store.template set<0>(reinterpret_cast<typename Storage::type>(address));
	}

//----------
public:
//----------

	//-----------------------------------------------------------------------------
	KAddrPlus(AddressOf* address = nullptr) { setAddress(address); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	AddressOf* operator->() const { return getAddress(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	AddressOf& operator*() const { return *getAddress(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	operator AddressOf*() const { return getAddress(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	AddressOf* get() const { return getAddress(); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	void set(AddressOf* value) { setAddress(value); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	template<unsigned pos>
	uint8_t get() const { return static_cast<uint8_t>(m_Store.template get<pos + 1>()); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	template<unsigned pos>
	void set(uint8_t value) { return m_Store.template set<pos + 1>(value); }
	//-----------------------------------------------------------------------------

	//-----------------------------------------------------------------------------
	KAddrPlus& operator=(AddressOf* address)
	//-----------------------------------------------------------------------------
	{
		setAddress(address);
		return *this;
	}

}; // KAddrPlus

} // end of namespace dekaf2

