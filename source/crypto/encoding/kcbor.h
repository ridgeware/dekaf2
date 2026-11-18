/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2026, Ridgeware, Inc.
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

/// @file kcbor.h
/// reading and writing of CBOR, the Concise Binary Object Representation

#include <dekaf2/core/init/kdefinitions.h>
#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/core/errors/kerror.h>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

DEKAF2_NAMESPACE_BEGIN

/// @addtogroup crypto_encoding
/// @{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// A data item of CBOR (RFC 8949): an integer, a floating point number, a string
/// of bytes or of text, an array, a map, a tagged data item, or a simple value
/// like true, false and null.
///
/// Other than in JSON, the key of a map entry can be of any type. COSE keys and
/// the CTAP2 structures of WebAuthn have maps with integers as keys, therefore a
/// map is a list of pairs in the order of the input.
///
/// @code
/// KCBOR Key;
///
/// if (Key.Parse(sInput))
/// {
///     auto  iAlgorithm = Key(3).GetInt64(); // the value of the map entry with the key 3
///     auto& sX         = Key(-2).GetString();
/// }
///
/// auto sOutput = KCBOR::Map().Add(1, 2).Add("name", KCBOR::Bytes(sBytes)).Serialize();
/// @endcode
class DEKAF2_PUBLIC KCBOR : public KErrorBase
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	/// the type of a data item
	enum class Type : uint8_t
	{
		Null,       ///< the simple value null
		Undefined,  ///< the simple value undefined
		Bool,       ///< the simple values true and false
		Unsigned,   ///< an integer from 0 to 2^64 - 1
		Negative,   ///< an integer from -2^64 to -1
		Float,      ///< a floating point number of half, single or double precision
		Bytes,      ///< a string of bytes
		Text,       ///< a string of text in UTF-8
		Array,      ///< a list of data items
		Map,        ///< a list of pairs of data items
		Tagged,     ///< a tag number with one data item
		Simple      ///< a simple value other than true, false, null and undefined
	};

	using ArrayType = std::vector<KCBOR>;
	using MapType   = std::vector<std::pair<KCBOR, KCBOR>>;

	/// the default for the count of arrays, maps and tags that may be nested in the input
	static constexpr uint16_t iDefaultMaxDepth = 64;

	/// construct a null value
	KCBOR () = default;

	/// construct a boolean
	template<typename T, typename std::enable_if<std::is_same<T, bool>::value, int>::type = 0>
	KCBOR (T bValue)
	: m_Type(Type::Bool)
	, m_iNumber(bValue)
	{
	}

	/// construct an integer
	template<typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
	KCBOR (T iValue)
	{
		SetInteger(static_cast<typename std::conditional<std::is_signed<T>::value, int64_t, uint64_t>::type>(iValue));
	}

	/// construct a floating point number
	template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
	KCBOR (T fValue)
	: m_Type(Type::Float)
	, m_fNumber(static_cast<double>(fValue))
	{
	}

	/// construct a string of text
	template<typename T, typename std::enable_if<std::is_convertible<const T&, KStringView>::value && !std::is_arithmetic<T>::value, int>::type = 0>
	KCBOR (const T& sText)
	: m_Type(Type::Text)
	, m_sString(KStringView(sText))
	{
	}

	/// @return a string of bytes
	DEKAF2_NODISCARD
	static KCBOR Bytes     (KStringView sBytes);

	/// @return an empty array
	DEKAF2_NODISCARD
	static KCBOR Array     ();

	/// @return an empty map
	DEKAF2_NODISCARD
	static KCBOR Map       ();

	/// @return a data item with a tag number
	DEKAF2_NODISCARD
	static KCBOR Tagged    (uint64_t iTag, KCBOR Content);

	/// @return a negative integer from the form that CBOR has for it, the distance below -1:
	/// the value is -1 - iOffset, so 0 gives -1, and 2^64 - 1 gives -2^64. The integers down
	/// to -2^63 are simpler to construct from an int64_t, this reaches the other half of the range.
	DEKAF2_NODISCARD
	static KCBOR Negative  (uint64_t iOffset);

	/// @return the simple value with this number. The numbers 20 to 23 are false, true, null and
	/// undefined. The numbers 24 to 31 are reserved and give null.
	DEKAF2_NODISCARD
	static KCBOR Simple    (uint8_t iValue);

	/// @return the simple value undefined
	DEKAF2_NODISCARD
	static KCBOR Undefined ();

	/// read the one data item that the input consists of
	/// @param sInput the encoded data item
	/// @param iMaxDepth the count of arrays, maps and tags that may be nested
	/// @return false if the input is not well-formed, is nested too deeply, or has bytes after
	/// the data item. The value is null then.
	bool Parse      (KStringView sInput, uint16_t iMaxDepth = iDefaultMaxDepth);

	/// read the data item at the start of the input, and remove its bytes from the input
	/// @param sInput the encoded data item, followed by any other bytes
	/// @param iMaxDepth the count of arrays, maps and tags that may be nested
	/// @return false if the input is not well-formed or is nested too deeply. The value is null
	/// then, and the input is unchanged.
	bool ParseFirst (KStringView& sInput, uint16_t iMaxDepth = iDefaultMaxDepth);

	/// @return the encoded data item. Integers and lengths are written in their shortest form,
	/// floating point numbers in the shortest form that keeps the value, and the entries of a
	/// map in their stored order.
	DEKAF2_NODISCARD
	KString Serialize () const;

	/// append the encoded data item to a string
	void Serialize (KStringRef& sOut) const;

	/// make this a null value. Does not touch the error state.
	void clear ();

	/// append a data item to an array. A value of another type becomes an empty array first.
	KCBOR& Add (KCBOR Value);

	/// append an entry to a map. A value of another type becomes an empty map first.
	KCBOR& Add (KCBOR Key, KCBOR Value);

	/// @return the value of the first map entry with this key, or a null value if this is not
	/// a map or has no such key
	DEKAF2_NODISCARD
	const KCBOR& operator() (const KCBOR& Key) const;

	/// @return true if this is a map with an entry of this key
	DEKAF2_NODISCARD
	bool Contains (const KCBOR& Key) const;

	DEKAF2_NODISCARD Type GetType     () const { return m_Type;                       }
	DEKAF2_NODISCARD bool IsNull      () const { return m_Type == Type::Null;         }
	DEKAF2_NODISCARD bool IsUndefined () const { return m_Type == Type::Undefined;    }
	DEKAF2_NODISCARD bool IsBool      () const { return m_Type == Type::Bool;         }
	DEKAF2_NODISCARD bool IsUnsigned  () const { return m_Type == Type::Unsigned;     }
	DEKAF2_NODISCARD bool IsNegative  () const { return m_Type == Type::Negative;     }
	DEKAF2_NODISCARD bool IsInteger   () const { return IsUnsigned() || IsNegative(); }
	DEKAF2_NODISCARD bool IsFloat     () const { return m_Type == Type::Float;        }
	DEKAF2_NODISCARD bool IsBytes     () const { return m_Type == Type::Bytes;        }
	DEKAF2_NODISCARD bool IsText      () const { return m_Type == Type::Text;         }
	DEKAF2_NODISCARD bool IsArray     () const { return m_Type == Type::Array;        }
	DEKAF2_NODISCARD bool IsMap       () const { return m_Type == Type::Map;          }
	DEKAF2_NODISCARD bool IsTagged    () const { return m_Type == Type::Tagged;       }
	DEKAF2_NODISCARD bool IsSimple    () const { return m_Type == Type::Simple;       }

	/// @return the value of a boolean, false for any other type
	DEKAF2_NODISCARD
	bool GetBool () const { return IsBool() && m_iNumber != 0; }

	/// @return the value of an integer, limited to the range of int64_t, and 0 for any other type
	DEKAF2_NODISCARD
	int64_t GetInt64 () const;

	/// @return the value of an unsigned integer, and 0 for any other type
	DEKAF2_NODISCARD
	uint64_t GetUInt64 () const { return IsUnsigned() ? m_iNumber : 0; }

	/// @return a negative integer in the form that CBOR has for it, the distance below -1: the
	/// value is -1 - offset. This holds the integers from -2^63 - 1 to -2^64 as well, which
	/// GetInt64() cannot return. 0 for any other type.
	DEKAF2_NODISCARD
	uint64_t GetNegativeOffset () const { return IsNegative() ? m_iNumber : 0; }

	/// @return the value of a floating point number, and 0 for any other type
	DEKAF2_NODISCARD
	double GetFloat () const { return m_fNumber; }

	/// @return the bytes of a string of bytes or of text, and an empty string for any other type
	DEKAF2_NODISCARD
	const KString& GetString () const { return m_sString; }

	/// @return the data items of an array, and an empty list for any other type
	DEKAF2_NODISCARD
	const ArrayType& GetArray () const;

	/// @return the entries of a map, and an empty list for any other type
	DEKAF2_NODISCARD
	const MapType& GetMap () const { return m_Map; }

	/// @return the tag number of a tagged data item, and 0 for any other type
	DEKAF2_NODISCARD
	uint64_t GetTag () const { return IsTagged() ? m_iNumber : 0; }

	/// @return the data item below a tag, and a null value for any other type
	DEKAF2_NODISCARD
	const KCBOR& GetTagged () const;

	/// @return the number of a simple value, 0 for any type that is not a simple value
	DEKAF2_NODISCARD
	uint8_t GetSimple () const;

	/// @return the count of data items of an array, of entries of a map, or of bytes of a
	/// string, and 0 for any other type
	DEKAF2_NODISCARD
	std::size_t size () const;

	/// two data items are equal if they have the same type and the same content. The integer 1
	/// and the floating point number 1.0 are not equal.
	friend DEKAF2_PUBLIC bool operator== (const KCBOR& left, const KCBOR& right);
	friend DEKAF2_PUBLIC bool operator!= (const KCBOR& left, const KCBOR& right) { return !(left == right); }

//----------
private:
//----------

	void SetInteger (int64_t  iValue);
	void SetInteger (uint64_t iValue);
	void SetSimple  (uint8_t  iValue);
	bool Read       (KStringView& sInput, uint16_t iDepth, KStringViewZ& sError);
	bool ReadChunks (KStringView& sInput, uint8_t iMajor, KStringViewZ& sError);

	// members that the type does not use stay in their initial state

	Type      m_Type    { Type::Null };
	uint64_t  m_iNumber { 0 };   // an unsigned integer, the offset of a negative one, a boolean, a tag or a simple value
	double    m_fNumber { 0 };
	KString   m_sString;         // a string of bytes or of text
	ArrayType m_Array;           // the data items of an array, or the one below a tag
	MapType   m_Map;

}; // KCBOR

/// @}

DEKAF2_NAMESPACE_END
