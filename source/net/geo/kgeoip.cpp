/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
// |/|   Software without restriction, including without limitation        |\|
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

#include <dekaf2/net/geo/kgeoip.h>
#include <dekaf2/core/format/kformat.h>
#include <dekaf2/core/logging/klog.h>
#include <cstring>
#include <ctime>
#include <vector>
#include <utility>

DEKAF2_NAMESPACE_BEGIN

namespace {

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
/// Decodes values from one section of a MaxMind DB file (the data section, or
/// the metadata section). All offsets are relative to the section base, which
/// is also what pointer values inside that section are relative to. The decoder
/// never owns memory - it reads straight from the memory mapped file - and is
/// bounds checked against malformed input throughout.
class MMDBDecoder
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
{

//----------
public:
//----------

	MMDBDecoder (const uint8_t* pBase, std::size_t iSize)
	: m_pBase (pBase)
	, m_iSize (iSize)
	{
	}

	/// find a key in the map at iMapOffset, returning the offset of its value
	bool FindKey (std::size_t iMapOffset, KStringView sKey, std::size_t& iValueOffset) const;

	/// read a UTF-8 string value as a zero-copy view into the section (follows pointers)
	DEKAF2_NODISCARD
	KStringView GetStringView (std::size_t iOffset) const;

	/// read a UTF-8 string value (follows pointers), returns an owned copy
	DEKAF2_NODISCARD
	KString GetString (std::size_t iOffset) const
	{
		return KString (GetStringView (iOffset));
	}

	/// read an unsigned integer value of any width (follows pointers)
	DEKAF2_NODISCARD
	uint64_t GetUInt (std::size_t iOffset) const;

	/// read a double or float value (follows pointers)
	DEKAF2_NODISCARD
	double GetDouble (std::size_t iOffset) const;

	/// read an array of UTF-8 strings (e.g. the metadata "languages" list)
	DEKAF2_NODISCARD
	std::vector<KString> GetStringArray (std::size_t iOffset) const
	{
		std::vector<KString> Result;

		iOffset = Resolve (iOffset);
		Control c = GetControl (iOffset);

		if (!c.bValid || c.iType != 11) // 11 = array
		{
			return Result;
		}

		std::size_t iOff = c.iPayload;

		for (std::size_t ii = 0; ii < c.iSize; ++ii)
		{
			Result.push_back (GetString (iOff));
			iOff = SkipValue (iOff);

			if (iOff >= m_iSize)
			{
				break;
			}
		}

		return Result;
	}

	/// return the section offset of each element of an array (e.g. "subdivisions"),
	/// so the caller can decode each element (typically a map) with FindKey()
	DEKAF2_NODISCARD
	std::vector<std::size_t> GetArrayElements (std::size_t iOffset) const
	{
		std::vector<std::size_t> Result;

		iOffset = Resolve (iOffset);
		Control c = GetControl (iOffset);

		if (!c.bValid || c.iType != 11) // 11 = array
		{
			return Result;
		}

		std::size_t iOff = c.iPayload;

		for (std::size_t ii = 0; ii < c.iSize; ++ii)
		{
			Result.push_back (iOff);
			iOff = SkipValue (iOff);

			if (iOff >= m_iSize)
			{
				break;
			}
		}

		return Result;
	}

	/// offset of the value of the first entry of a map (e.g. the first available
	/// language of a "names" map). Returns false if it is not a map or is empty.
	DEKAF2_NODISCARD
	bool FirstValue (std::size_t iMapOffset, std::size_t& iValueOffset) const
	{
		iMapOffset = Resolve (iMapOffset);
		Control c = GetControl (iMapOffset);

		if (!c.bValid || c.iType != 7 || c.iSize == 0) // 7 = map, needs >= 1 entry
		{
			return false;
		}

		iValueOffset = SkipValue (c.iPayload); // skip the first key -> first value
		return iValueOffset < m_iSize;
	}

	/// read a boolean value (follows pointers)
	DEKAF2_NODISCARD
	bool GetBool (std::size_t iOffset) const
	{
		iOffset = Resolve (iOffset);
		Control c = GetControl (iOffset);
		return c.bValid && c.iType == 14 && c.iSize != 0; // 14 = boolean, value in the size field
	}

	/// iterate the entries of a map, calling fn(KStringView key, std::size_t valueOffset)
	/// for each entry. Does nothing if iMapOffset does not point at a map.
	template<class Fn>
	void ForEachInMap (std::size_t iMapOffset, Fn&& fn) const
	{
		iMapOffset = Resolve (iMapOffset);
		Control c = GetControl (iMapOffset);

		if (!c.bValid || c.iType != 7) // 7 = map
		{
			return;
		}

		std::size_t iOff = c.iPayload;

		for (std::size_t ii = 0; ii < c.iSize; ++ii)
		{
			KStringView sKey      = GetStringView (iOff);
			iOff                  = SkipValue (iOff); // advance past the key -> value
			std::size_t iValueOff = iOff;

			fn (sKey, iValueOff);

			iOff = SkipValue (iOff); // advance past the value -> next key

			if (iOff >= m_iSize)
			{
				break;
			}
		}
	}

//----------
private:
//----------

	/// a decoded control byte (plus any size / pointer extension bytes)
	struct Control
	{
		uint8_t     iType    { 0 };       ///< MMDB data type (1 == pointer)
		bool        bPointer { false };   ///< true if this value is a pointer
		bool        bValid   { false };   ///< false if reading ran out of bounds
		std::size_t iSize    { 0 };       ///< payload bytes, or element/pair count for array/map
		std::size_t iPayload { 0 };       ///< offset of the payload, or of the value after a pointer
		std::size_t iPointer { 0 };       ///< pointer target offset (only if bPointer)
	};

	DEKAF2_NODISCARD Control     GetControl    (std::size_t iOffset) const;
	DEKAF2_NODISCARD std::size_t Resolve       (std::size_t iOffset) const;
	DEKAF2_NODISCARD std::size_t SkipValue     (std::size_t iOffset) const;

	const uint8_t* m_pBase;
	std::size_t    m_iSize;

}; // MMDBDecoder

//-----------------------------------------------------------------------------
MMDBDecoder::Control MMDBDecoder::GetControl (std::size_t iOffset) const
//-----------------------------------------------------------------------------
{
	Control c;

	if (iOffset >= m_iSize)
	{
		return c; // bValid stays false
	}

	uint8_t iCtrl = m_pBase[iOffset];
	++iOffset;

	uint8_t iType = static_cast<uint8_t>(iCtrl >> 5);

	if (iType == 0)
	{
		// extended type: the real type is the next byte plus 7
		if (iOffset >= m_iSize)
		{
			return c;
		}
		iType = static_cast<uint8_t>(m_pBase[iOffset] + 7);
		++iOffset;
	}

	c.iType = iType;

	if (iType == 1)
	{
		// pointer - bits 3 and 4 of the control byte select the pointer width
		auto iPSize = static_cast<uint8_t>((iCtrl >> 3) & 0x03);
		auto iNeed  = static_cast<std::size_t>(iPSize) + 1;

		if (iOffset + iNeed > m_iSize)
		{
			return c;
		}

		uint32_t iPtr = 0;

		switch (iPSize)
		{
			case 0:
				iPtr = (static_cast<uint32_t>(iCtrl & 0x07) << 8)
				     |  static_cast<uint32_t>(m_pBase[iOffset]);
				break;

			case 1:
				iPtr = ((static_cast<uint32_t>(iCtrl & 0x07)         << 16)
				     |  (static_cast<uint32_t>(m_pBase[iOffset])     << 8)
				     |   static_cast<uint32_t>(m_pBase[iOffset + 1])) + 2048;
				break;

			case 2:
				iPtr = ((static_cast<uint32_t>(iCtrl & 0x07)         << 24)
				     |  (static_cast<uint32_t>(m_pBase[iOffset])     << 16)
				     |  (static_cast<uint32_t>(m_pBase[iOffset + 1]) << 8)
				     |   static_cast<uint32_t>(m_pBase[iOffset + 2])) + 526336;
				break;

			default: // 3
				iPtr =  (static_cast<uint32_t>(m_pBase[iOffset])     << 24)
				     |  (static_cast<uint32_t>(m_pBase[iOffset + 1]) << 16)
				     |  (static_cast<uint32_t>(m_pBase[iOffset + 2]) << 8)
				     |   static_cast<uint32_t>(m_pBase[iOffset + 3]);
				break;
		}

		c.bPointer = true;
		c.bValid   = true;
		c.iPointer = iPtr;
		c.iPayload = iOffset + iNeed; // the value following the pointer
		return c;
	}

	std::size_t iSize = static_cast<std::size_t>(iCtrl & 0x1f);

	if (iSize >= 29)
	{
		if (iSize == 29)
		{
			if (iOffset + 1 > m_iSize) { return c; }
			iSize = 29 + m_pBase[iOffset];
			iOffset += 1;
		}
		else if (iSize == 30)
		{
			if (iOffset + 2 > m_iSize) { return c; }
			iSize = 285 + ((static_cast<std::size_t>(m_pBase[iOffset]) << 8)
			            |   static_cast<std::size_t>(m_pBase[iOffset + 1]));
			iOffset += 2;
		}
		else // 31
		{
			if (iOffset + 3 > m_iSize) { return c; }
			iSize = 65821 + ((static_cast<std::size_t>(m_pBase[iOffset])     << 16)
			              |  (static_cast<std::size_t>(m_pBase[iOffset + 1]) << 8)
			              |   static_cast<std::size_t>(m_pBase[iOffset + 2]));
			iOffset += 3;
		}
	}

	c.iSize    = iSize;
	c.iPayload = iOffset;
	c.bValid   = true;
	return c;

} // GetControl

//-----------------------------------------------------------------------------
std::size_t MMDBDecoder::Resolve (std::size_t iOffset) const
//-----------------------------------------------------------------------------
{
	// follow a (short) chain of pointers to the actual value, guarding against cycles
	for (int ii = 0; ii < 8; ++ii)
	{
		Control c = GetControl (iOffset);

		if (!c.bValid || !c.bPointer)
		{
			return iOffset;
		}

		iOffset = c.iPointer;
	}

	return iOffset;

} // Resolve

//-----------------------------------------------------------------------------
std::size_t MMDBDecoder::SkipValue (std::size_t iOffset) const
//-----------------------------------------------------------------------------
{
	Control c = GetControl (iOffset);

	if (!c.bValid)
	{
		return m_iSize; // push callers out of bounds, stopping any iteration
	}

	if (c.bPointer)
	{
		return c.iPayload;
	}

	switch (c.iType)
	{
		case 7: // map
		{
			std::size_t iOff = c.iPayload;
			for (std::size_t ii = 0; ii < c.iSize; ++ii)
			{
				iOff = SkipValue (iOff); // key
				iOff = SkipValue (iOff); // value
				if (iOff >= m_iSize) { break; }
			}
			return iOff;
		}

		case 11: // array
		{
			std::size_t iOff = c.iPayload;
			for (std::size_t ii = 0; ii < c.iSize; ++ii)
			{
				iOff = SkipValue (iOff);
				if (iOff >= m_iSize) { break; }
			}
			return iOff;
		}

		case 14: // boolean - the value is in the size field, there are no payload bytes
			return c.iPayload;

		default: // string, bytes, double, float, (u)int*
			return c.iPayload + c.iSize;
	}

} // SkipValue

//-----------------------------------------------------------------------------
KStringView MMDBDecoder::GetStringView (std::size_t iOffset) const
//-----------------------------------------------------------------------------
{
	iOffset = Resolve (iOffset);

	Control c = GetControl (iOffset);

	if (!c.bValid || c.iType != 2 || c.iPayload + c.iSize > m_iSize)
	{
		return KStringView();
	}

	return KStringView (reinterpret_cast<const char*>(m_pBase + c.iPayload), c.iSize);

} // GetStringView

//-----------------------------------------------------------------------------
uint64_t MMDBDecoder::GetUInt (std::size_t iOffset) const
//-----------------------------------------------------------------------------
{
	iOffset = Resolve (iOffset);

	Control c = GetControl (iOffset);

	if (!c.bValid || c.bPointer || c.iPayload + c.iSize > m_iSize)
	{
		return 0;
	}

	uint64_t    iValue = 0;
	std::size_t iLen   = (c.iSize < 8) ? c.iSize : 8;

	for (std::size_t ii = 0; ii < iLen; ++ii)
	{
		iValue = (iValue << 8) | m_pBase[c.iPayload + ii];
	}

	return iValue;

} // GetUInt

//-----------------------------------------------------------------------------
double MMDBDecoder::GetDouble (std::size_t iOffset) const
//-----------------------------------------------------------------------------
{
	iOffset = Resolve (iOffset);

	Control c = GetControl (iOffset);

	if (!c.bValid)
	{
		return 0.0;
	}

	if (c.iType == 3 && c.iSize == 8 && c.iPayload + 8 <= m_iSize)
	{
		uint64_t iBits = 0;
		for (int ii = 0; ii < 8; ++ii)
		{
			iBits = (iBits << 8) | m_pBase[c.iPayload + ii];
		}
		double nValue;
		std::memcpy (&nValue, &iBits, sizeof(nValue));
		return nValue;
	}

	if (c.iType == 15 && c.iSize == 4 && c.iPayload + 4 <= m_iSize)
	{
		uint32_t iBits = 0;
		for (int ii = 0; ii < 4; ++ii)
		{
			iBits = (iBits << 8) | m_pBase[c.iPayload + ii];
		}
		float nValue;
		std::memcpy (&nValue, &iBits, sizeof(nValue));
		return nValue;
	}

	return 0.0;

} // GetDouble

//-----------------------------------------------------------------------------
bool MMDBDecoder::FindKey (std::size_t iMapOffset, KStringView sKey, std::size_t& iValueOffset) const
//-----------------------------------------------------------------------------
{
	iMapOffset = Resolve (iMapOffset);

	Control c = GetControl (iMapOffset);

	if (!c.bValid || c.iType != 7)
	{
		return false;
	}

	std::size_t iOff = c.iPayload;

	for (std::size_t ii = 0; ii < c.iSize; ++ii)
	{
		KStringView sThisKey = GetStringView (iOff);
		iOff = SkipValue (iOff); // advance past the key

		if (sThisKey == sKey)
		{
			iValueOffset = iOff;
			return true;
		}

		iOff = SkipValue (iOff); // advance past the value

		if (iOff >= m_iSize)
		{
			break;
		}
	}

	return false;

} // FindKey

} // anonymous namespace

//-----------------------------------------------------------------------------
bool KGeoIP::Open (KStringViewZ sDatabaseFile)
//-----------------------------------------------------------------------------
{
	Close ();
	ClearError ();

	if (!m_File.Open (sDatabaseFile))
	{
		return SetError (m_File.CopyLastError());
	}

	KStringView sFile = m_File.ToView();

	// the metadata section starts right after this marker, near the end of the file
	static constexpr char s_szMarker[] =
		{ '\xab', '\xcd', '\xef', 'M', 'a', 'x', 'M', 'i', 'n', 'd', '.', 'c', 'o', 'm' };
	KStringView sMarker (s_szMarker, sizeof(s_szMarker));

	auto iMarkerPos = sFile.rfind (sMarker);

	if (iMarkerPos == KStringView::npos)
	{
		Close ();
		return SetError (kFormat ("'{}': not a MaxMind DB file (metadata marker not found)", sDatabaseFile));
	}

	std::size_t iMetaStart = iMarkerPos + sMarker.size();

	// the metadata is a single MMDB map, encoded just like the data section
	MMDBDecoder oMeta (reinterpret_cast<const uint8_t*>(sFile.data()) + iMetaStart,
	                   sFile.size() - iMetaStart);

	std::size_t iValue;

	if (oMeta.FindKey (0, "node_count",    iValue))
	{
		m_iNodeCount = static_cast<uint32_t>(oMeta.GetUInt (iValue));
	}

	if (oMeta.FindKey (0, "record_size",   iValue))
	{
		m_iRecordSize = static_cast<uint16_t>(oMeta.GetUInt (iValue));
	}

	if (oMeta.FindKey (0, "ip_version",    iValue))
	{
		m_iIPVersion = static_cast<uint16_t>(oMeta.GetUInt (iValue));
	}

	if (oMeta.FindKey (0, "build_epoch",   iValue))
	{
		m_tBuildTime = KUnixTime::from_time_t (static_cast<std::time_t>(oMeta.GetUInt (iValue)));
	}

	if (oMeta.FindKey (0, "database_type", iValue))
	{
		m_sDatabaseType = oMeta.GetString (iValue);
	}

	if (oMeta.FindKey (0, "languages",     iValue))
	{
		m_Languages = oMeta.GetStringArray (iValue);
	}

	if (m_iRecordSize != 24 && m_iRecordSize != 28 && m_iRecordSize != 32)
	{
		auto iBadSize = m_iRecordSize;
		Close ();
		return SetError (kFormat ("'{}': unsupported record size {}", sDatabaseFile, iBadSize));
	}

	if (m_iNodeCount == 0)
	{
		Close ();
		return SetError (kFormat ("'{}': empty search tree", sDatabaseFile));
	}

	m_iRecordLength = (static_cast<std::size_t>(m_iRecordSize) * 2) / 8;

	std::size_t iTreeSize = static_cast<std::size_t>(m_iNodeCount) * m_iRecordLength;
	std::size_t iDataFrom = iTreeSize + 16; // 16 byte data section separator

	if (iDataFrom > iMarkerPos)
	{
		Close ();
		return SetError (kFormat ("'{}': corrupt database (search tree exceeds file size)", sDatabaseFile));
	}

	m_pTree        = reinterpret_cast<const uint8_t*>(sFile.data());
	m_pDataSection = m_pTree + iDataFrom;
	m_iDataSize    = iMarkerPos - iDataFrom; // the data section ends where the metadata marker begins

	// pre-compute the node where the IPv4 subtree begins. An IPv6 database stores
	// IPv4 data under the ::/96 prefix, reached by following the left (bit 0) record
	// 96 times from the root - so an IPv4 lookup can start there and walk 32 bits.
	m_iIPv4StartNode = 0;

	if (m_iIPVersion == 6)
	{
		uint32_t iNode = 0;
		for (int ii = 0; ii < 96 && iNode < m_iNodeCount; ++ii)
		{
			iNode = ReadRecord (iNode, 0);
		}
		m_iIPv4StartNode = iNode;
	}

	kDebug (1, "opened '{}' (type '{}'), {} nodes, {}-bit records, ip_version {}",
	        sDatabaseFile, m_sDatabaseType, m_iNodeCount, m_iRecordSize, m_iIPVersion);

	return true;

} // Open

//-----------------------------------------------------------------------------
void KGeoIP::Close ()
//-----------------------------------------------------------------------------
{
	m_File.Close ();
	m_pTree          = nullptr;
	m_pDataSection   = nullptr;
	m_iDataSize      = 0;
	m_iRecordLength  = 0;
	m_iNodeCount     = 0;
	m_iIPv4StartNode = 0;
	m_iRecordSize    = 0;
	m_iIPVersion     = 0;
	m_tBuildTime     = KUnixTime();
	m_sDatabaseType.clear ();
	m_Languages.clear (); // note: m_sDefaultLanguage is configuration and is kept

} // Close

//-----------------------------------------------------------------------------
uint32_t KGeoIP::ReadRecord (uint32_t iNode, uint32_t iBit) const
//-----------------------------------------------------------------------------
{
	const uint8_t* p = m_pTree + static_cast<std::size_t>(iNode) * m_iRecordLength;

	switch (m_iRecordSize)
	{
		case 24:
			return (iBit == 0)
				? (static_cast<uint32_t>(p[0]) << 16) | (static_cast<uint32_t>(p[1]) << 8) | p[2]
				: (static_cast<uint32_t>(p[3]) << 16) | (static_cast<uint32_t>(p[4]) << 8) | p[5];

		case 28:
			// the shared middle byte holds the high nibble of each record
			return (iBit == 0)
				? (static_cast<uint32_t>(p[3] & 0xF0) << 20) | (static_cast<uint32_t>(p[0]) << 16) | (static_cast<uint32_t>(p[1]) << 8) | p[2]
				: (static_cast<uint32_t>(p[3] & 0x0F) << 24) | (static_cast<uint32_t>(p[4]) << 16) | (static_cast<uint32_t>(p[5]) << 8) | p[6];

		case 32:
			return (iBit == 0)
				? (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3]
				: (static_cast<uint32_t>(p[4]) << 24) | (static_cast<uint32_t>(p[5]) << 16) | (static_cast<uint32_t>(p[6]) << 8) | p[7];
	}

	return m_iNodeCount; // unreachable - record size is validated in Open()

} // ReadRecord

//-----------------------------------------------------------------------------
uint32_t KGeoIP::FindNode (const uint8_t* pAddress, int iStartBit, int iBitCount, uint32_t iStartNode) const
//-----------------------------------------------------------------------------
{
	uint32_t iNode = iStartNode;

	for (int iBit = iStartBit; iBit < iBitCount && iNode < m_iNodeCount; ++iBit)
	{
		int      iByte   = iBit >> 3;
		uint32_t iBitVal = (static_cast<uint32_t>(pAddress[iByte]) >> (7 - (iBit & 7))) & 1;
		iNode = ReadRecord (iNode, iBitVal);
	}

	return iNode;

} // FindNode

//-----------------------------------------------------------------------------
KString KGeoIP::NormalizeLanguage (KStringView sLanguage)
//-----------------------------------------------------------------------------
{
	// BCP-47 case conventions: the language (primary) subtag is lower case, the region
	// subtag is upper case. Geo databases use only language[-region] (no script subtags),
	// so we lower case up to the first '-' and upper case the rest.
	auto iDash = sLanguage.find ('-');

	if (iDash == KStringView::npos)
	{
		return KString (sLanguage).MakeLower();
	}

	KString sResult (sLanguage.substr (0, iDash));
	sResult.MakeLower();
	sResult += '-';
	sResult += KString (sLanguage.substr (iDash + 1)).MakeUpper();
	return sResult;

} // NormalizeLanguage

//-----------------------------------------------------------------------------
bool KGeoIP::FindDataOffset (const KIPAddress& IP, std::size_t& iDataOffset) const
//-----------------------------------------------------------------------------
{
	uint32_t iNode;

	if (IP.Is4())
	{
		const auto& Bytes = IP.ToBytes4();
		// in an IPv6 database the IPv4 lookup starts at the pre-computed ::/96 node
		iNode = FindNode (Bytes.data(), 0, 32, (m_iIPVersion == 6) ? m_iIPv4StartNode : 0);
	}
	else
	{
		if (m_iIPVersion != 6)
		{
			return false; // an IPv6 address cannot be resolved in an IPv4-only database
		}

		const auto& Bytes = IP.ToBytes6();
		iNode = FindNode (Bytes.data(), 0, 128, 0);
	}

	if (iNode <= m_iNodeCount)
	{
		// == node_count is the explicit "not found" record; < node_count means the
		// address was shorter than the tree depth - either way there is no data
		return false;
	}

	uint64_t iPointer = static_cast<uint64_t>(iNode) - m_iNodeCount;

	if (iPointer < 16)
	{
		return false; // below the first legal data offset
	}

	std::size_t iOffset = static_cast<std::size_t>(iPointer - 16);

	if (iOffset >= m_iDataSize)
	{
		return false; // out of bounds
	}

	iDataOffset = iOffset;
	return true;

} // FindDataOffset

//-----------------------------------------------------------------------------
void KGeoIP::DecodeRecord (std::size_t iDataOffset, LocationView& View, KStringView sLanguage, bool bWantStrings) const
//-----------------------------------------------------------------------------
{
	MMDBDecoder oData (m_pDataSection, m_iDataSize);

	// the primary subtag of a language tag is everything before the first '-'
	// (e.g. "pt" of "pt-BR"). dekaf2 has no ICU locale data, but for matching a
	// requested language range against the available tags this is all we need.
	auto PrimarySubtag = [] (KStringView sLang) -> KStringView
	{
		auto iDash = sLang.find ('-');
		return (iDash == KStringView::npos) ? sLang : sLang.substr (0, iDash);
	};

	// match one language tag against the "names" keys: exact match first, then a
	// primary-subtag match per RFC 4647 lookup (so "pt" matches "pt-BR", and a missing
	// "pt-BR" still matches another "pt-*" variant - at least one is picked)
	auto FindForLang = [&] (std::size_t iNamesMap, KStringView sLang, std::size_t& iValueOffset) -> bool
	{
		if (oData.FindKey (iNamesMap, sLang, iValueOffset))
		{
			return true;
		}

		KStringView sPrimary = PrimarySubtag (sLang);
		bool        bFound   = false;

		oData.ForEachInMap (iNamesMap, [&] (KStringView sKey, std::size_t iValue)
		{
			if (!bFound && PrimarySubtag (sKey) == sPrimary)
			{
				iValueOffset = iValue;
				bFound       = true;
			}
		});

		return bFound;
	};

	// resolve a localized name from a "names" map: requested -> default -> first available
	auto Localized = [&] (std::size_t iNames) -> KStringView
	{
		std::size_t iName;

		if (FindForLang (iNames, sLanguage, iName))
		{
			return oData.GetStringView (iName);
		}
		if (sLanguage != m_sDefaultLanguage && FindForLang (iNames, m_sDefaultLanguage, iName))
		{
			return oData.GetStringView (iName);
		}
		if (oData.FirstValue (iNames, iName))
		{
			return oData.GetStringView (iName); // any language the record carries
		}

		return KStringView();
	};

	// one pass over the record: the numeric LocationIDs base is always filled, the
	// string fields only when requested (so LookupIDs reads no strings at all)
	oData.ForEachInMap (iDataOffset, [&] (KStringView sKey, std::size_t iValue)
	{
		switch (sKey.Hash())
		{
			case "continent"_hash:
				oData.ForEachInMap (iValue, [&] (KStringView sK, std::size_t iV)
				{
					switch (sK.Hash())
					{
						case "geoname_id"_hash:
							View.iContinentGeoNameID = static_cast<uint32_t>(oData.GetUInt (iV));
							break;

						case "code"_hash:
							if (bWantStrings)
							{
								View.sContinent = oData.GetStringView (iV);
							}
							break;

						case "names"_hash:
							if (bWantStrings)
							{
								View.sContinentName = Localized (iV);
							}
							break;
					}
				});
				break;

			case "country"_hash:
				oData.ForEachInMap (iValue, [&] (KStringView sK, std::size_t iV)
				{
					switch (sK.Hash())
					{
						case "geoname_id"_hash:
							View.iCountryGeoNameID = static_cast<uint32_t>(oData.GetUInt (iV));
							break;

						case "is_in_european_union"_hash:
							View.bIsInEU = oData.GetBool (iV);
							break;

						case "iso_code"_hash:
							if (bWantStrings)
							{
								View.sCountryISO  = oData.GetStringView (iV);
							}
							break;

						case "names"_hash:
							if (bWantStrings)
							{
								View.sCountryName = Localized (iV);
							}
							break;
					}
				});
				break;

			case "city"_hash:
				oData.ForEachInMap (iValue, [&] (KStringView sK, std::size_t iV)
				{
					switch (sK.Hash())
					{
						case "geoname_id"_hash:
							View.iCityGeoNameID = static_cast<uint32_t>(oData.GetUInt (iV));
							break;

						case "names"_hash:
							if (bWantStrings)
							{
								View.sCity = Localized (iV);
							}
							break;
					}
				});
				break;

			case "subdivisions"_hash:
				for (auto iElement : oData.GetArrayElements (iValue))
				{
					uint32_t        iGeoNameID = 0;
					SubdivisionView Sub;

					oData.ForEachInMap (iElement, [&] (KStringView sK, std::size_t iV)
					{
						switch (sK.Hash())
						{
							case "geoname_id"_hash:
								iGeoNameID = static_cast<uint32_t>(oData.GetUInt (iV));
								break;

							case "iso_code"_hash:
								if (bWantStrings)
								{
									Sub.sISO  = oData.GetStringView (iV);
								}
								break;

							case "names"_hash:
								if (bWantStrings)
								{
									Sub.sName = Localized (iV);
								}
								break;
						}
					});

					View.SubdivisionGeoNameIDs.push_back (iGeoNameID);

					if (bWantStrings)
					{
						View.Subdivisions.push_back (std::move(Sub));
					}
				}
				break;

			case "postal"_hash:
				if (bWantStrings)
				{
					oData.ForEachInMap (iValue, [&] (KStringView sK, std::size_t iV)
					{
						if (sK.Hash() == "code"_hash)
						{
							View.sPostalCode = oData.GetStringView (iV);
						}
					});
				}
				break;

			case "location"_hash:
				oData.ForEachInMap (iValue, [&] (KStringView sK, std::size_t iV)
				{
					switch (sK.Hash())
					{
						case "latitude"_hash:
							View.nLatitude = oData.GetDouble (iV);
							break;

						case "longitude"_hash:
							View.nLongitude = oData.GetDouble (iV);
							break;

						case "accuracy_radius"_hash:
							View.iAccuracyRadius = static_cast<uint32_t>(oData.GetUInt (iV));
							break;

						case "time_zone"_hash:
							if (bWantStrings)
							{
								View.sTimeZone = oData.GetStringView (iV);
							}
							break;
					}
				});
				break;
		}
	});

} // DecodeRecord

//-----------------------------------------------------------------------------
KGeoIP::Location::Location (LocationIDs&& Identity, const LocationView& View)
//-----------------------------------------------------------------------------
: LocationIDs    (std::move(Identity)) // the public ctors built this by copy or move from the view
, sContinent     (View.sContinent)     // a view owns no bytes -> its strings are always copied
, sContinentName (View.sContinentName)
, sCountryISO    (View.sCountryISO)
, sCountryName   (View.sCountryName)
, sCity          (View.sCity)
, sPostalCode    (View.sPostalCode)
, sTimeZone      (View.sTimeZone)
{
	Subdivisions.reserve (View.Subdivisions.size());

	for (const auto& Sub : View.Subdivisions)
	{
		Subdivisions.push_back (Subdivision { KString(Sub.sISO), KString(Sub.sName) });
	}

} // Location

//-----------------------------------------------------------------------------
KGeoIP::LocationView KGeoIP::LookupView (const KIPAddress& IP, KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	LocationView View;

	if (!is_open())
	{
		SetError ("no database open");
		return View;
	}

	std::size_t iDataOffset;

	if (FindDataOffset (IP, iDataOffset))
	{
		// empty language means "use the configured default" (already normalized); a
		// requested tag is normalized to BCP-47 case so it matches the database keys
		KString     sRequested = sLanguage.empty() ? KString{} : NormalizeLanguage (sLanguage);
		KStringView sLang      = sLanguage.empty() ? KStringView (m_sDefaultLanguage) : KStringView (sRequested);
		DecodeRecord (iDataOffset, View, sLang, true);
	}

	return View;

} // LookupView

//-----------------------------------------------------------------------------
KGeoIP::LocationView KGeoIP::LookupView (KStringView sIP, KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	KIPError   ec;
	KIPAddress IP (sIP, ec);

	if (ec)
	{
		SetError (kFormat ("invalid IP address '{}': {}", sIP, ec.message()));
		return LocationView();
	}

	return LookupView (IP, sLanguage);

} // LookupView

//-----------------------------------------------------------------------------
KGeoIP::Location KGeoIP::Lookup (const KIPAddress& IP, KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	return Location (LookupView (IP, sLanguage));

} // Lookup

//-----------------------------------------------------------------------------
KGeoIP::Location KGeoIP::Lookup (KStringView sIP, KStringView sLanguage) const
//-----------------------------------------------------------------------------
{
	return Location (LookupView (sIP, sLanguage));

} // Lookup

//-----------------------------------------------------------------------------
KGeoIP::LocationIDs KGeoIP::LookupIDs (const KIPAddress& IP) const
//-----------------------------------------------------------------------------
{
	LocationView View;

	if (!is_open())
	{
		SetError ("no database open");
		return View; // empty -> slices to an empty LocationIDs
	}

	std::size_t iDataOffset;

	if (FindDataOffset (IP, iDataOffset))
	{
		DecodeRecord (iDataOffset, View, KStringView{}, false); // numeric fields only, no strings
	}

	// returns the view sliced to its LocationIDs base. Returning the named local is an
	// implicit move under C++20 (the base, incl. its SubdivisionGeoNameIDs vector, is moved;
	// under C++17/14 the small base is merely copied) - so no explicit std::move.
	return View;

} // LookupIDs

//-----------------------------------------------------------------------------
KGeoIP::LocationIDs KGeoIP::LookupIDs (KStringView sIP) const
//-----------------------------------------------------------------------------
{
	KIPError   ec;
	KIPAddress IP (sIP, ec);

	if (ec)
	{
		SetError (kFormat ("invalid IP address '{}': {}", sIP, ec.message()));
		return LocationIDs();
	}

	return LookupIDs (IP);

} // LookupIDs

DEKAF2_NAMESPACE_END
