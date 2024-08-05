/*
 //
 // DEKAF(tm): Lighter, Faster, Smarter(tm)
 //
 // Copyright (c) 2024, Ridgeware, Inc.
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

/// @file kbuffer.h
/// const and mutable buffers for conversion between C and C++ APIs

#include "kdefinitions.h"
#include "kstringview.h"
#include <cstdint>

DEKAF2_NAMESPACE_BEGIN

namespace detail {

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
class KBufferTraitsRaw
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	KBufferTraitsRaw() = default;
	KBufferTraitsRaw(void* Data, size_type iCapacity) 
	: m_Data(static_cast<char*>(Data)), m_iCapacity(iCapacity) {}

	/// returns the overall size of the buffer
	std::size_t capacity() const { return m_iCapacity; }
	/// returns buffer as a const char pointer
	const char* data()     const { return m_Data;      }
	/// returns buffer as a char pointer
	char*       data()           { return m_Data;      }
	/// returns true if the buffer is nullptr
	bool        is_null()  const { return !data();     }

//----------
protected:
//----------

	char*       m_Data      { nullptr };
	size_type   m_iCapacity { 0 };

}; // KBufferTraitsRaw

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
template<std::size_t iCapacity>
class KBufferTraitsArray
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	KBufferTraitsArray() = default;

	/// returns the overall size of the buffer
	std::size_t capacity() const { return m_Data.size(); }
	/// returns buffer as a const char pointer
	const char* data()     const { return m_Data.data(); }
	/// returns buffer as a char pointer
	char*       data()           { return m_Data.data(); }
	/// returns true if the buffer is nullptr
	bool        is_null()  const { return false;         }

//----------
protected:
//----------

	std::array<char, iCapacity> m_Data;

}; // KBufferTraitsArray

} // end of namespace detail

class KConstBuffer;

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class that points to writeable memory, and keeps track of capacity and current fill size.
/// We add conversions to easily interface between a C API and C++ objects
template<bool bThrow, class Traits = detail::KBufferTraitsRaw>
class KBufferBase : public Traits
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;
	using self = KBufferBase<bThrow, Traits>;

	KBufferBase () = default;
	/// construct writeable Buffer from data and capacity
	KBufferBase (void* Data, size_type iCapacity) : Traits(Data, iCapacity) {}

	/// returns buffer as a void pointer
	void*       VoidData    ()                                  { return data(); }
	/// returns buffer as a char pointer
	char*       CharData    ()                                  { return data(); }
	/// returns buffer as a uint8_t pointer
	uint8_t*    UInt8Data   ()                                  { return static_cast<uint8_t*>(VoidData()); }
	/// returns buffer as a KStringView (returns only filled size)
	KStringView ToView      ()                                  { return KStringView(data(), size());       }
	/// returns buffer as a KStringView (returns full buffer size (capacity))
	KStringView ToFullView  ()                                  { return KStringView(Traits::data(), capacity()); }

	/// returns buffer as a char pointer
	char*       data        ()                                  { return Traits::data() + get_consumed();   }
	/// return the overall size of the buffer
	size_type   capacity    () const                            { return Traits::capacity(); }
	/// return the filled size of the buffer
	size_type   size        () const                            { return get_filled() - get_consumed();     }
	/// return true if the overall size is 0
	bool        empty       () const                            { return !size();   }
	/// resets Buffer to empty state (keeps the buffer though)
	void        reset       ()                                  { set_filled(0); set_consumed(0); }
	/// set a new filled buffer size - returns the new filled size, may be less than requested
	/// if buffer size had been exceeded
	size_type   resize      (size_type iSize)                   { set_filled(std::min(iSize, capacity())); return get_filled(); }
	/// returns true if the buffer is nullptr
	bool        is_null     () const                            { return Traits::is_null();   }
	/// return the remaining size of the buffer, which is the difference between the overall size and the filled size
	size_type   remaining   () const                            { return capacity() - get_filled(); }
	/// advance the data pointer by the count of consumed bytes, e.g. after a read operation. The returned actual consumed count
	/// may be less than requested if consumable size had been exceeded
	size_type   consume     (size_type iConsumed)               { return add_consumed(std::min(iConsumed, size())); }

	/// append len bytes from a void ptr until buffer is full - returns count of copied bytes
	size_type   append      (const void* Data, size_type iSize)
	{
		if (!Data || is_null()) return 0;
		auto iCopy = std::min(remaining(), iSize);
		std::memcpy(data() + get_filled(), Data, iCopy);
		add_filled(iCopy);
		return iCopy;
	}
	/// append from the filled and unconsumed part of a Buffer until buffer is full - returns count of copied bytes
	template<bool B, class T>
	size_type   append      (KBufferBase<B, T> Buffer)          { return append(Buffer.data(), Buffer.size()); }
	/// append from a KConstBuffer until buffer is full - returns count of copied bytes
	size_type   append      (KConstBuffer buffer);
	/// append from a KStringView until buffer is full - returns count of copied bytes
	size_type   append      (KStringView sData)                 { return append(sData.data(), sData.size());   }
	/// append a char, returns 1 if successful
	size_type   append      (char ch)
	{
		if (size() == capacity()) return 0;
		*(data() + m_iFilled++) = ch;
		return 1;
	}

	/// assign len bytes from a void ptr until buffer is full - returns count of copied bytes
	size_type   assign      (const void* Data, size_type iSize) { reset(); return append(Data, iSize); }
	/// assign from the filled and unconsumed part of a Buffer until buffer is full - returns count of copied bytes
	template<bool B, class T>
	size_type   assign      (KBufferBase<B, T> Buffer)          { reset(); return append(Buffer);      }
	/// assign from a KConstBuffer until buffer is full - returns count of copied bytes
	size_type   assign      (KConstBuffer buffer);
	/// assign from a KStringView until buffer is full - returns count of copied bytes
	size_type   assign      (KStringView sData)                 { reset(); return append(sData);       }
	/// assign a char, returns 1 if successful
	size_type   assign      (char ch)                           { reset(); return append(ch);          }

	/// append from the filled and unconsumed part of a Buffer until buffer is full - returns count of copied bytes
	template<bool B, class T>
	size_type   push_back   (KBufferBase<B, T> Buffer)          { return append(Buffer); }
	/// append from a KConstBuffer until buffer is full - returns count of copied bytes
	size_type   push_back   (KConstBuffer Buffer);
	/// append from a KStringView until buffer is full - returns count of copied bytes
	size_type   push_back   (KStringView sData)                 { return append(sData);  }
	/// append a char, returns 1 if successful
	size_type   push_back   (char ch)                           { return append(ch);     }

	template<bool B, class T>
	self& operator += (KBufferBase<B, T> Buffer)                { append(Buffer); return *this; }
	self& operator += (KStringView sData)                       { append(sData);  return *this; }
	self& operator += (char ch)                                 { append(ch);     return *this; }
	self& operator += (KConstBuffer Buffer);

	template<bool B, class T>
	self& operator = (KBufferBase<B, T> Buffer)                 { assign(Buffer); return *this; }
	self& operator = (KStringView sData)                        { assign(sData);  return *this; }
	self& operator = (char ch)                                  { assign(ch);     return *this; }
	self& operator = (KConstBuffer Buffer);

	operator bool () const                                      { return !is_null(); }

//----------
protected:
//----------

	// unchecked raw counter manipulations
	size_type   get_filled   () const                           { return m_iFilled;    }
	size_type   get_consumed () const                           { return m_iConsumed;  }
	void        set_filled   (size_type size)                   { m_iFilled    = size; }
	void        set_consumed (size_type size)                   { m_iConsumed  = size; }
	size_type   add_filled   (size_type size)                   { m_iFilled   += size; return size; }
	size_type   add_consumed (size_type size)                   { m_iConsumed += size; return size; }

//----------
private:
//----------

	size_type   m_iFilled   { 0 };
	size_type   m_iConsumed { 0 };

}; // KBufferBase

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class without own storage
using KBuffer = KBufferBase<false, detail::KBufferTraitsRaw>;
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class without own storage
using KThrowingBuffer = KBufferBase<true, detail::KBufferTraitsRaw>;
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class with a template argument defined buffer size
template<std::size_t iCapacity, bool bThrow = false>
using KReservedBuffer = KBufferBase<bThrow, detail::KBufferTraitsArray<iCapacity>>;
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Buffer class that points to const memory and can be used for reading. Used to interface between
/// C API and C++ objects.
class KConstBuffer
//:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	using size_type = std::size_t;

	KConstBuffer () = default;
	/// construct readable Buffer from data and len
	KConstBuffer (const void* data, size_type len) : m_Data(static_cast<const char*>(data)), m_iSize(len) {}
	/// construct readable Buffer from KStringView
	KConstBuffer (KStringView sData)               : m_Data(sData.data()), m_iSize(sData.size()) {}
	/// construct readable Buffer from (writeable) Buffer, uses only the filled size of the buffer
	template<bool B, class T>
	KConstBuffer (KBufferBase<B, T> buffer)        : m_Data(buffer.data()), m_iSize(buffer.size()) {}

	/// returns buffer as a const void pointer
	const void*     VoidData   () const { return data();  }
	/// returns buffer as a const char pointer
	const char*     CharData   () const { return data();  }
	/// returns buffer as a const uint8_t pointer
	const uint8_t*  UInt8Data  () const { return static_cast<const uint8_t*>(VoidData()); }
	/// returns buffer as a KStringView
	KStringView     ToView     () const { return KStringView{ data(), size() }; }

	/// returns buffer as a char pointer
	const char*     data       () const { return m_Data;  }
	/// returns size of the buffer
	size_type       size       () const { return m_iSize; }
	/// return true if the overall size is 0
	bool            empty      () const { return !size(); }

	operator        bool       () const { return data();  }

//----------
private:
//----------

	const char* m_Data  { nullptr };
	size_type   m_iSize { 0 };

}; // KConstBuffer

//-----------------------------------------------------------------------------
template<bool B, class T>
KBufferBase<B, T>::size_type KBufferBase<B, T>::append(KConstBuffer Buffer)
//-----------------------------------------------------------------------------
{
	return append(Buffer.VoidData(), Buffer.size());
}

//-----------------------------------------------------------------------------
template<bool B, class T>
KBufferBase<B, T>::size_type KBufferBase<B, T>::assign(KConstBuffer Buffer)
//-----------------------------------------------------------------------------
{
	return assign(Buffer.VoidData(), Buffer.size());
}

//-----------------------------------------------------------------------------
template<bool B, class T>
KBufferBase<B, T>::size_type KBufferBase<B, T>::push_back(KConstBuffer Buffer)
//-----------------------------------------------------------------------------
{
	return append(Buffer);
}

//-----------------------------------------------------------------------------
template<bool B, class T>
KBufferBase<B, T>& KBufferBase<B, T>::operator += (KConstBuffer Buffer)
//-----------------------------------------------------------------------------
{
	append(Buffer);
	return *this;
}

//-----------------------------------------------------------------------------
template<bool B, class T>
KBufferBase<B, T>& KBufferBase<B, T>::operator = (KConstBuffer Buffer)
//-----------------------------------------------------------------------------
{
	assign(Buffer);
	return *this;
}

DEKAF2_NAMESPACE_END
