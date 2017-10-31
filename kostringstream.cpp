/*
//-----------------------------------------------------------------------------//
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

#include "kostringstream.h"

namespace dekaf2
{

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream::KOStringStream(KOStringStream&& other)
    : m_sBuf{other.m_sBuf}
    , m_KOStreamBuf{std::move(other.m_KOStreamBuf)}
//-----------------------------------------------------------------------------
{
} // move ctor
#endif

//-----------------------------------------------------------------------------
KOStringStream::~KOStringStream()
//-----------------------------------------------------------------------------
{}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream& KOStringStream::operator=(KOStringStream&& other)
//-----------------------------------------------------------------------------
{
	m_sBuf = other.m_sBuf;
	m_KOStreamBuf = std::move(other.m_KOStreamBuf);
	return *this;
}
#endif

//-----------------------------------------------------------------------------
/// this "restarts" the buffer, like a call to the constructor
bool KOStringStream::open(KString& str)
//-----------------------------------------------------------------------------
{
	*m_sBuf = str;
}

//-----------------------------------------------------------------------------
/// this is the custom KString writer
std::streamsize KOStringStream::KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (sTargetBuf != nullptr && sBuffer != nullptr)
	{
		const KString* pInBuf = reinterpret_cast<const KString *>(sBuffer);
		KString* pOutBuf = reinterpret_cast<KString *>(sTargetBuf);
		*pOutBuf += *pInBuf;
		iWrote = pInBuf->size();
	}
	return iWrote;
}

} // end namespace dekaf2
