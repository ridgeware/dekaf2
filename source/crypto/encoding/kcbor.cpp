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


#include <dekaf2/crypto/encoding/kcbor.h>
#include <cmath>
#include <cstring>
#include <limits>

DEKAF2_NAMESPACE_BEGIN

static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
              "KCBOR needs the floating point formats of IEEE 754");

namespace {

// the major types of CBOR, the upper three bits of the first byte of a data item
enum class Major : uint8_t
{
	Unsigned = 0,
	Negative = 1,
	Bytes    = 2,
	Text     = 3,
	Array    = 4,
	Map      = 5,
	Tag      = 6,
	Simple   = 7
};

// the values of the lower five bits of the first byte
constexpr uint8_t iOneByteArgument = 24;
constexpr uint8_t iHalfFloat       = 25;
constexpr uint8_t iSingleFloat     = 26;
constexpr uint8_t iDoubleFloat     = 27;
constexpr uint8_t iIndefinite      = 31;

// the byte that ends a string, an array or a map of indefinite length
constexpr uint8_t iBreak           = 0xff;

constexpr uint8_t iSimpleFalse     = 20;
constexpr uint8_t iSimpleTrue      = 21;
constexpr uint8_t iSimpleNull      = 22;
constexpr uint8_t iSimpleUndefined = 23;

constexpr KStringViewZ sTruncated      { "the input ends inside of a data item"                         };
constexpr KStringViewZ sReserved       { "a data item has one of the reserved values 28 to 30"          };
constexpr KStringViewZ sBadIndefinite  { "an integer or a tag has an indefinite length"                 };
constexpr KStringViewZ sBadChunk       { "a string of indefinite length has a chunk that is no string of the same type with a definite length" };
constexpr KStringViewZ sBadBreak       { "a break without an array, a map or a string of indefinite length" };
constexpr KStringViewZ sBadSimple      { "a simple value below 32 is written with two bytes"            };
constexpr KStringViewZ sTooDeep        { "arrays, maps and tags are nested too deeply"                  };
constexpr KStringViewZ sTrailingBytes  { "the input has bytes after the data item"                      };

//-----------------------------------------------------------------------------
void WriteBigEndian(KStringRef& sOut, uint64_t iValue, uint8_t iBytes)
//-----------------------------------------------------------------------------
{
	while (iBytes > 0)
	{
		--iBytes;
		sOut += static_cast<char>(static_cast<uint8_t>(iValue >> (iBytes * 8)));
	}

} // WriteBigEndian

//-----------------------------------------------------------------------------
/// writes the first byte of a data item and its argument in the shortest form
void WriteHead(KStringRef& sOut, Major major, uint64_t iArgument)
//-----------------------------------------------------------------------------
{
	auto iMajor = static_cast<uint8_t>(static_cast<uint8_t>(major) << 5);

	if (iArgument < iOneByteArgument)
	{
		sOut += static_cast<char>(static_cast<uint8_t>(iMajor | iArgument));
	}
	else if (iArgument <= 0xff)
	{
		sOut += static_cast<char>(static_cast<uint8_t>(iMajor | iOneByteArgument));
		WriteBigEndian(sOut, iArgument, 1);
	}
	else if (iArgument <= 0xffff)
	{
		sOut += static_cast<char>(static_cast<uint8_t>(iMajor | (iOneByteArgument + 1)));
		WriteBigEndian(sOut, iArgument, 2);
	}
	else if (iArgument <= 0xffffffff)
	{
		sOut += static_cast<char>(static_cast<uint8_t>(iMajor | (iOneByteArgument + 2)));
		WriteBigEndian(sOut, iArgument, 4);
	}
	else
	{
		sOut += static_cast<char>(static_cast<uint8_t>(iMajor | (iOneByteArgument + 3)));
		WriteBigEndian(sOut, iArgument, 8);
	}

} // WriteHead

//-----------------------------------------------------------------------------
/// reads the argument that follows the first byte of a data item
bool ReadArgument(KStringView& sInput, uint8_t iInfo, uint64_t& iArgument, KStringViewZ& sError)
//-----------------------------------------------------------------------------
{
	if (iInfo < iOneByteArgument)
	{
		iArgument = iInfo;
		return true;
	}

	if (iInfo > iDoubleFloat)
	{
		sError = sReserved;
		return false;
	}

	auto iBytes = std::size_t(1) << (iInfo - iOneByteArgument);

	if (sInput.size() < iBytes)
	{
		sError = sTruncated;
		return false;
	}

	iArgument = 0;

	for (std::size_t iPos = 0; iPos < iBytes; ++iPos)
	{
		iArgument = (iArgument << 8) | static_cast<uint8_t>(sInput[iPos]);
	}

	sInput.remove_prefix(iBytes);

	return true;

} // ReadArgument

//-----------------------------------------------------------------------------
double HalfToDouble(uint16_t iHalf)
//-----------------------------------------------------------------------------
{
	int    iExponent = (iHalf >> 10) & 0x1f;
	int    iMantissa = iHalf & 0x3ff;
	double fValue;

	if (iExponent == 0)
	{
		// zero and the subnormal numbers
		fValue = std::ldexp(static_cast<double>(iMantissa), -24);
	}
	else if (iExponent != 31)
	{
		fValue = std::ldexp(static_cast<double>(iMantissa + 1024), iExponent - 25);
	}
	else if (iMantissa == 0)
	{
		fValue = std::numeric_limits<double>::infinity();
	}
	else
	{
		fValue = std::numeric_limits<double>::quiet_NaN();
	}

	return (iHalf & 0x8000) ? -fValue : fValue;

} // HalfToDouble

//-----------------------------------------------------------------------------
/// converts the bits of a number of single precision into those of half precision
/// @return false if half precision cannot hold the value
bool SingleToHalf(uint32_t iSingle, uint16_t& iHalf)
//-----------------------------------------------------------------------------
{
	auto     iSign     = static_cast<uint16_t>((iSingle >> 16) & 0x8000);
	int      iExponent = static_cast<int>((iSingle >> 23) & 0xff);
	uint32_t iMantissa = iSingle & 0x7fffff;

	if (iExponent == 0)
	{
		// zero fits, the subnormal numbers of single precision are too small
		iHalf = iSign;
		return iMantissa == 0;
	}

	if (iExponent == 0xff)
	{
		// infinity, the caller handles NaN
		iHalf = static_cast<uint16_t>(iSign | 0x7c00);
		return iMantissa == 0;
	}

	// remove the bias
	iExponent -= 127;

	if (iExponent >= -14 && iExponent <= 15)
	{
		// a normal number of half precision has ten bits of mantissa
		iHalf = static_cast<uint16_t>(iSign | ((iExponent + 15) << 10) | (iMantissa >> 13));
		return (iMantissa & 0x1fff) == 0;
	}

	if (iExponent >= -24 && iExponent <= -15)
	{
		// a subnormal number of half precision counts in units of 2^-24
		auto iShift = static_cast<unsigned>(-iExponent - 1);
		auto iFull  = iMantissa | 0x800000;
		iHalf = static_cast<uint16_t>(iSign | (iFull >> iShift));
		return (iFull & ((uint32_t(1) << iShift) - 1)) == 0;
	}

	return false;

} // SingleToHalf

//-----------------------------------------------------------------------------
/// writes a floating point number in the shortest form that keeps its value
void WriteFloat(KStringRef& sOut, double fValue)
//-----------------------------------------------------------------------------
{
	auto iMajor = static_cast<uint8_t>(static_cast<uint8_t>(Major::Simple) << 5);

	if (std::isnan(fValue))
	{
		sOut += static_cast<char>(static_cast<uint8_t>(iMajor | iHalfFloat));
		WriteBigEndian(sOut, 0x7e00, 2);
		return;
	}

	// a conversion to single precision is only defined for values in its range
	if (std::isinf(fValue) || std::fabs(fValue) <= static_cast<double>(std::numeric_limits<float>::max()))
	{
		auto fSingle = static_cast<float>(fValue);

		if (static_cast<double>(fSingle) == fValue)
		{
			uint32_t iSingle;
			std::memcpy(&iSingle, &fSingle, sizeof(iSingle));

			uint16_t iHalf;

			if (SingleToHalf(iSingle, iHalf))
			{
				sOut += static_cast<char>(static_cast<uint8_t>(iMajor | iHalfFloat));
				WriteBigEndian(sOut, iHalf, 2);
			}
			else
			{
				sOut += static_cast<char>(static_cast<uint8_t>(iMajor | iSingleFloat));
				WriteBigEndian(sOut, iSingle, 4);
			}

			return;
		}
	}

	uint64_t iDouble;
	std::memcpy(&iDouble, &fValue, sizeof(iDouble));

	sOut += static_cast<char>(static_cast<uint8_t>(iMajor | iDoubleFloat));
	WriteBigEndian(sOut, iDouble, 8);

} // WriteFloat

//-----------------------------------------------------------------------------
const KCBOR& NullValue()
//-----------------------------------------------------------------------------
{
	static const KCBOR s_Null;
	return s_Null;
}

} // end of anonymous namespace

//-----------------------------------------------------------------------------
KCBOR KCBOR::Bytes(KStringView sBytes)
//-----------------------------------------------------------------------------
{
	KCBOR Value;
	Value.m_Type    = Type::Bytes;
	Value.m_sString = sBytes;
	return Value;
}

//-----------------------------------------------------------------------------
KCBOR KCBOR::Array()
//-----------------------------------------------------------------------------
{
	KCBOR Value;
	Value.m_Type = Type::Array;
	return Value;
}

//-----------------------------------------------------------------------------
KCBOR KCBOR::Map()
//-----------------------------------------------------------------------------
{
	KCBOR Value;
	Value.m_Type = Type::Map;
	return Value;
}

//-----------------------------------------------------------------------------
KCBOR KCBOR::Tagged(uint64_t iTag, KCBOR Content)
//-----------------------------------------------------------------------------
{
	KCBOR Value;
	Value.m_Type    = Type::Tagged;
	Value.m_iNumber = iTag;
	Value.m_Array.push_back(std::move(Content));
	return Value;
}

//-----------------------------------------------------------------------------
KCBOR KCBOR::Negative(uint64_t iOffset)
//-----------------------------------------------------------------------------
{
	KCBOR Value;
	Value.m_Type    = Type::Negative;
	Value.m_iNumber = iOffset;
	return Value;
}

//-----------------------------------------------------------------------------
KCBOR KCBOR::Simple(uint8_t iValue)
//-----------------------------------------------------------------------------
{
	KCBOR Value;

	if (iValue < iOneByteArgument || iValue > iIndefinite)
	{
		Value.SetSimple(iValue);
	}

	return Value;
}

//-----------------------------------------------------------------------------
KCBOR KCBOR::Undefined()
//-----------------------------------------------------------------------------
{
	KCBOR Value;
	Value.m_Type = Type::Undefined;
	return Value;
}

//-----------------------------------------------------------------------------
void KCBOR::SetInteger(int64_t iValue)
//-----------------------------------------------------------------------------
{
	if (iValue < 0)
	{
		m_Type    = Type::Negative;
		m_iNumber = static_cast<uint64_t>(-(iValue + 1));
	}
	else
	{
		m_Type    = Type::Unsigned;
		m_iNumber = static_cast<uint64_t>(iValue);
	}
}

//-----------------------------------------------------------------------------
void KCBOR::SetInteger(uint64_t iValue)
//-----------------------------------------------------------------------------
{
	m_Type    = Type::Unsigned;
	m_iNumber = iValue;
}

//-----------------------------------------------------------------------------
void KCBOR::SetSimple(uint8_t iValue)
//-----------------------------------------------------------------------------
{
	switch (iValue)
	{
		case iSimpleFalse:
		case iSimpleTrue:
			m_Type    = Type::Bool;
			m_iNumber = (iValue == iSimpleTrue);
			break;

		case iSimpleNull:
			m_Type    = Type::Null;
			break;

		case iSimpleUndefined:
			m_Type    = Type::Undefined;
			break;

		default:
			m_Type    = Type::Simple;
			m_iNumber = iValue;
			break;
	}

} // SetSimple

//-----------------------------------------------------------------------------
void KCBOR::clear()
//-----------------------------------------------------------------------------
{
	m_Type    = Type::Null;
	m_iNumber = 0;
	m_fNumber = 0;
	m_sString.clear();
	m_Array.clear();
	m_Map.clear();
}

//-----------------------------------------------------------------------------
KCBOR& KCBOR::Add(KCBOR Value)
//-----------------------------------------------------------------------------
{
	if (!IsArray())
	{
		clear();
		m_Type = Type::Array;
	}

	m_Array.push_back(std::move(Value));

	return *this;
}

//-----------------------------------------------------------------------------
KCBOR& KCBOR::Add(KCBOR Key, KCBOR Value)
//-----------------------------------------------------------------------------
{
	if (!IsMap())
	{
		clear();
		m_Type = Type::Map;
	}

	m_Map.emplace_back(std::move(Key), std::move(Value));

	return *this;
}

//-----------------------------------------------------------------------------
const KCBOR& KCBOR::operator()(const KCBOR& Key) const
//-----------------------------------------------------------------------------
{
	for (auto& Entry : m_Map)
	{
		if (Entry.first == Key)
		{
			return Entry.second;
		}
	}

	return NullValue();
}

//-----------------------------------------------------------------------------
bool KCBOR::Contains(const KCBOR& Key) const
//-----------------------------------------------------------------------------
{
	for (auto& Entry : m_Map)
	{
		if (Entry.first == Key)
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
int64_t KCBOR::GetInt64() const
//-----------------------------------------------------------------------------
{
	constexpr auto iMax = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());

	switch (m_Type)
	{
		case Type::Unsigned:
			return (m_iNumber > iMax) ? std::numeric_limits<int64_t>::max()
			                          : static_cast<int64_t>(m_iNumber);

		case Type::Negative:
			return (m_iNumber > iMax) ? std::numeric_limits<int64_t>::min()
			                          : -1 - static_cast<int64_t>(m_iNumber);

		case Type::Null:
		case Type::Undefined:
		case Type::Bool:
		case Type::Float:
		case Type::Bytes:
		case Type::Text:
		case Type::Array:
		case Type::Map:
		case Type::Tagged:
		case Type::Simple:
			break;
	}

	return 0;

} // GetInt64

//-----------------------------------------------------------------------------
const KCBOR::ArrayType& KCBOR::GetArray() const
//-----------------------------------------------------------------------------
{
	// a tagged data item keeps its content in the array, too
	return IsArray() ? m_Array : NullValue().m_Array;
}

//-----------------------------------------------------------------------------
const KCBOR& KCBOR::GetTagged() const
//-----------------------------------------------------------------------------
{
	return (IsTagged() && !m_Array.empty()) ? m_Array.front() : NullValue();
}

//-----------------------------------------------------------------------------
uint8_t KCBOR::GetSimple() const
//-----------------------------------------------------------------------------
{
	switch (m_Type)
	{
		case Type::Null:
			return iSimpleNull;

		case Type::Undefined:
			return iSimpleUndefined;

		case Type::Bool:
			return m_iNumber ? iSimpleTrue : iSimpleFalse;

		case Type::Simple:
			return static_cast<uint8_t>(m_iNumber);

		case Type::Unsigned:
		case Type::Negative:
		case Type::Float:
		case Type::Bytes:
		case Type::Text:
		case Type::Array:
		case Type::Map:
		case Type::Tagged:
			break;
	}

	return 0;

} // GetSimple

//-----------------------------------------------------------------------------
std::size_t KCBOR::size() const
//-----------------------------------------------------------------------------
{
	switch (m_Type)
	{
		case Type::Bytes:
		case Type::Text:
			return m_sString.size();

		case Type::Array:
			return m_Array.size();

		case Type::Map:
			return m_Map.size();

		case Type::Null:
		case Type::Undefined:
		case Type::Bool:
		case Type::Unsigned:
		case Type::Negative:
		case Type::Float:
		case Type::Tagged:
		case Type::Simple:
			break;
	}

	return 0;

} // size

//-----------------------------------------------------------------------------
bool operator==(const KCBOR& left, const KCBOR& right)
//-----------------------------------------------------------------------------
{
	// the members that a type does not use are in their initial state
	return left.m_Type    == right.m_Type
	    && left.m_iNumber == right.m_iNumber
	    && left.m_fNumber == right.m_fNumber
	    && left.m_sString == right.m_sString
	    && left.m_Array   == right.m_Array
	    && left.m_Map     == right.m_Map;
}

//-----------------------------------------------------------------------------
bool KCBOR::ReadChunks(KStringView& sInput, uint8_t iMajor, KStringViewZ& sError)
//-----------------------------------------------------------------------------
{
	for (;;)
	{
		if (sInput.empty())
		{
			sError = sTruncated;
			return false;
		}

		auto iInitial = static_cast<uint8_t>(sInput.front());
		sInput.remove_prefix(1);

		if (iInitial == iBreak)
		{
			return true;
		}

		uint8_t iInfo = iInitial & 0x1f;

		if ((iInitial >> 5) != iMajor || iInfo == iIndefinite)
		{
			sError = sBadChunk;
			return false;
		}

		uint64_t iLength { 0 };

		if (!ReadArgument(sInput, iInfo, iLength, sError))
		{
			return false;
		}

		if (iLength > sInput.size())
		{
			sError = sTruncated;
			return false;
		}

		m_sString += sInput.substr(0, static_cast<std::size_t>(iLength));
		sInput.remove_prefix(static_cast<std::size_t>(iLength));
	}

} // ReadChunks

//-----------------------------------------------------------------------------
bool KCBOR::Read(KStringView& sInput, uint16_t iDepth, KStringViewZ& sError)
//-----------------------------------------------------------------------------
{
	// this value is null when it gets here

	if (sInput.empty())
	{
		sError = sTruncated;
		return false;
	}

	auto iInitial = static_cast<uint8_t>(sInput.front());
	sInput.remove_prefix(1);

	auto     major       = static_cast<Major>(iInitial >> 5);
	uint8_t  iInfo       = iInitial & 0x1f;
	bool     bIndefinite = (iInfo == iIndefinite);
	uint64_t iArgument { 0 };

	if (!bIndefinite && !ReadArgument(sInput, iInfo, iArgument, sError))
	{
		return false;
	}

	switch (major)
	{
		case Major::Unsigned:
		case Major::Negative:
		{
			if (bIndefinite)
			{
				sError = sBadIndefinite;
				return false;
			}

			m_Type    = (major == Major::Unsigned) ? Type::Unsigned : Type::Negative;
			m_iNumber = iArgument;

			return true;
		}

		case Major::Bytes:
		case Major::Text:
		{
			m_Type = (major == Major::Bytes) ? Type::Bytes : Type::Text;

			if (bIndefinite)
			{
				return ReadChunks(sInput, static_cast<uint8_t>(major), sError);
			}

			if (iArgument > sInput.size())
			{
				sError = sTruncated;
				return false;
			}

			m_sString = sInput.substr(0, static_cast<std::size_t>(iArgument));
			sInput.remove_prefix(static_cast<std::size_t>(iArgument));

			return true;
		}

		case Major::Array:
		{
			m_Type = Type::Array;

			if (iDepth == 0)
			{
				sError = sTooDeep;
				return false;
			}

			// every data item has at least one byte
			if (!bIndefinite && iArgument > sInput.size())
			{
				sError = sTruncated;
				return false;
			}

			for (uint64_t iCount = 0; bIndefinite || iCount < iArgument; ++iCount)
			{
				if (bIndefinite && !sInput.empty() && static_cast<uint8_t>(sInput.front()) == iBreak)
				{
					sInput.remove_prefix(1);
					break;
				}

				m_Array.emplace_back();

				if (!m_Array.back().Read(sInput, iDepth - 1, sError))
				{
					return false;
				}
			}

			return true;
		}

		case Major::Map:
		{
			m_Type = Type::Map;

			if (iDepth == 0)
			{
				sError = sTooDeep;
				return false;
			}

			// every entry has at least two bytes
			if (!bIndefinite && iArgument > sInput.size() / 2)
			{
				sError = sTruncated;
				return false;
			}

			for (uint64_t iCount = 0; bIndefinite || iCount < iArgument; ++iCount)
			{
				if (bIndefinite && !sInput.empty() && static_cast<uint8_t>(sInput.front()) == iBreak)
				{
					sInput.remove_prefix(1);
					break;
				}

				m_Map.emplace_back();

				auto& Entry = m_Map.back();

				if (!Entry.first .Read(sInput, iDepth - 1, sError) ||
				    !Entry.second.Read(sInput, iDepth - 1, sError))
				{
					return false;
				}
			}

			return true;
		}

		case Major::Tag:
		{
			if (bIndefinite)
			{
				sError = sBadIndefinite;
				return false;
			}

			if (iDepth == 0)
			{
				sError = sTooDeep;
				return false;
			}

			m_Type    = Type::Tagged;
			m_iNumber = iArgument;
			m_Array.emplace_back();

			return m_Array.back().Read(sInput, iDepth - 1, sError);
		}

		case Major::Simple:
		{
			if (bIndefinite)
			{
				sError = sBadBreak;
				return false;
			}

			if (iInfo == iHalfFloat)
			{
				m_Type    = Type::Float;
				m_fNumber = HalfToDouble(static_cast<uint16_t>(iArgument));
			}
			else if (iInfo == iSingleFloat)
			{
				auto  iSingle = static_cast<uint32_t>(iArgument);
				float fSingle;
				std::memcpy(&fSingle, &iSingle, sizeof(fSingle));

				m_Type    = Type::Float;
				m_fNumber = static_cast<double>(fSingle);
			}
			else if (iInfo == iDoubleFloat)
			{
				m_Type    = Type::Float;
				std::memcpy(&m_fNumber, &iArgument, sizeof(m_fNumber));
			}
			else if (iInfo == iOneByteArgument && iArgument < 32)
			{
				sError = sBadSimple;
				return false;
			}
			else
			{
				SetSimple(static_cast<uint8_t>(iArgument));
			}

			return true;
		}
	}

	return false;

} // Read

//-----------------------------------------------------------------------------
bool KCBOR::ParseFirst(KStringView& sInput, uint16_t iMaxDepth)
//-----------------------------------------------------------------------------
{
	ClearError();
	clear();

	auto         sRest = sInput;
	KStringViewZ sError;

	if (!Read(sRest, iMaxDepth, sError))
	{
		clear();
		return SetError(sError);
	}

	sInput = sRest;

	return true;

} // ParseFirst

//-----------------------------------------------------------------------------
bool KCBOR::Parse(KStringView sInput, uint16_t iMaxDepth)
//-----------------------------------------------------------------------------
{
	if (!ParseFirst(sInput, iMaxDepth))
	{
		return false;
	}

	if (!sInput.empty())
	{
		clear();
		return SetError(sTrailingBytes);
	}

	return true;

} // Parse

//-----------------------------------------------------------------------------
void KCBOR::Serialize(KStringRef& sOut) const
//-----------------------------------------------------------------------------
{
	switch (m_Type)
	{
		case Type::Null:
		case Type::Undefined:
		case Type::Bool:
		case Type::Simple:
			WriteHead(sOut, Major::Simple, GetSimple());
			break;

		case Type::Unsigned:
			WriteHead(sOut, Major::Unsigned, m_iNumber);
			break;

		case Type::Negative:
			WriteHead(sOut, Major::Negative, m_iNumber);
			break;

		case Type::Float:
			WriteFloat(sOut, m_fNumber);
			break;

		case Type::Bytes:
			WriteHead(sOut, Major::Bytes, m_sString.size());
			sOut.append(m_sString.data(), m_sString.size());
			break;

		case Type::Text:
			WriteHead(sOut, Major::Text, m_sString.size());
			sOut.append(m_sString.data(), m_sString.size());
			break;

		case Type::Array:
			WriteHead(sOut, Major::Array, m_Array.size());

			for (auto& Value : m_Array)
			{
				Value.Serialize(sOut);
			}
			break;

		case Type::Map:
			WriteHead(sOut, Major::Map, m_Map.size());

			for (auto& Entry : m_Map)
			{
				Entry.first .Serialize(sOut);
				Entry.second.Serialize(sOut);
			}
			break;

		case Type::Tagged:
			WriteHead(sOut, Major::Tag, m_iNumber);
			GetTagged().Serialize(sOut);
			break;
	}

} // Serialize

//-----------------------------------------------------------------------------
KString KCBOR::Serialize() const
//-----------------------------------------------------------------------------
{
	KString sOut;
	Serialize(sOut);
	return sOut;
}

DEKAF2_NAMESPACE_END
