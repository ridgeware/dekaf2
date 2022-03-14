/*
//
// DEKAF(tm): Lighter, Faster, Smarter(tm)
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
//
*/

#include "ksql_dbc.h"
#include "../ksystem.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
DBCFileBase::~DBCFileBase()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool DBCFileBase::IntSetBuffer(KStringView sBuffer, void* pBuffer, std::size_t iExpectedSize)
//-----------------------------------------------------------------------------
{
	if (sBuffer.size() == iExpectedSize)
	{
		memcpy(pBuffer, sBuffer.data(), sBuffer.size());
		return true;
	}
	return false;

} // DBCFileBase::IntSetBuffer

//-----------------------------------------------------------------------------
bool DBCFileBase::IntSave(KStringViewZ sFilename, const void* pBuffer, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	KOutFile File(sFilename);
	if (File.is_open())
	{
		File.Write(pBuffer, iSize);
		return File.Good();
	}
	return false;

} // DBCFileBase::IntSave

//-----------------------------------------------------------------------------
void DBCFileBase::decrypt (KStringRef& sString)
//-----------------------------------------------------------------------------
{
	unsigned char* string = reinterpret_cast<unsigned char*>(sString.data());
	size_t iLen = sString.size();
	for (size_t ii=0; ii < iLen; ++ii)
	{
		string[ii] -= 127;
	}

} // DBCFileBase::decrypt

//-----------------------------------------------------------------------------
void DBCFileBase::encrypt (char* string)
//-----------------------------------------------------------------------------
{
	size_t iLen = strlen(string);
	for (size_t ii=0; ii < iLen; ++ii)
	{
		string[ii] += 127;
	}

} // DBCFileBase::encrypt

//-----------------------------------------------------------------------------
int32_t DBCFileBase::DecodeInt(const uint8_t (&iSourceArray)[4])
//-----------------------------------------------------------------------------
{
	return (iSourceArray[3]<<24) + (iSourceArray[2]<<16) + (iSourceArray[1]<<8) + iSourceArray[0];

} // DBCFileBase::DecodeInt

//-----------------------------------------------------------------------------
void DBCFileBase::EncodeInt(uint8_t (&cTargetArray)[4], int32_t iSourceValue)
//-----------------------------------------------------------------------------
{
	// EncodeInt encodes a source int32_t integer into a four-byte little endian target

	const auto iTargetSize = sizeof(cTargetArray);
	auto iRemainingSourceBits = iSourceValue;

	for (uint32_t iTargetIndex = 0; iTargetIndex < iTargetSize; ++iTargetIndex)
	{
		cTargetArray[iTargetIndex] = static_cast<uint32_t>(iRemainingSourceBits) & 0x000000ff;
		iRemainingSourceBits >>= 8;
	}

} // DBCFileBase::EncodeInt

//-----------------------------------------------------------------------------
KString DBCFileBase::GetString(const void* pStr, uint16_t iMaxLen)
//-----------------------------------------------------------------------------
{
	auto szStr = static_cast<const char*>(pStr);
	KString sString( szStr, strnlen(szStr, iMaxLen) );
	decrypt(sString);
	return sString;

} // DBCFileBase::GetString

//-----------------------------------------------------------------------------
bool DBCFileBase::SetString(void* pStr, KStringView sStr, uint16_t iMaxLen)
//-----------------------------------------------------------------------------
{
	if (sStr.size() <= iMaxLen)
	{
		auto szStr = static_cast<char*>(pStr);
		std::strncpy(szStr, sStr.data(), iMaxLen);
		szStr[iMaxLen] = 0;
		encrypt(szStr);
		return true;
	}
	else
	{
		return false;
	}

} // DBCFileBase::SetString

//-----------------------------------------------------------------------------
uint16_t DBCFileBase::DecodeVersion(const void* pStr)
//-----------------------------------------------------------------------------
{
	auto szStr = static_cast<const char*>(pStr);
	auto iLen  = strnlen(szStr, 10);
	auto vCh   = szStr[iLen-1];
	return vCh - '0';
} // DBCFileBase::DecodeVersion

//-----------------------------------------------------------------------------
void DBCFileBase::FillBufferWithNoise(void* pBuffer, std::size_t iSize)
//-----------------------------------------------------------------------------
{
	auto iBuffer = static_cast<uint32_t*>(pBuffer);
	for (std::size_t iCt = 0; iCt < iSize / sizeof(uint32_t); ++ iCt)
	{
		iBuffer[iCt] = static_cast<uint32_t>(kRandom(0, UINT32_MAX));
	}

} // DBCFileBase::FillBufferWithNoise

} // namespace dekaf2
