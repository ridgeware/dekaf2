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

#include "kstringstream.h"

namespace dekaf2
{

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KOStringStream::KOStringStream(KOStringStream&& other)
    : base_type{std::move(other)}
    , m_sBuf{other.m_sBuf}
    , m_KOStreamBuf{std::move(other.m_KOStreamBuf)}
//-----------------------------------------------------------------------------
{
} // move ctor
#endif

//-----------------------------------------------------------------------------
KOStringStream::~KOStringStream()
//-----------------------------------------------------------------------------
{}

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
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
	return true;
}

//-----------------------------------------------------------------------------
/// this is the custom KString writer
std::streamsize KOStringStream::KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (sTargetBuf != nullptr && sBuffer != nullptr)
	{
		const KString::value_type* pInBuf = reinterpret_cast<const KString::value_type*>(sBuffer);
		KString** pOutBuf = reinterpret_cast<KString**>(sTargetBuf);
		if (*pOutBuf != nullptr)
		{
			(*pOutBuf)->append(pInBuf, static_cast<size_t>(iCount));
			iWrote = iCount;
		}
	}

	return iWrote;
}




#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KIStringStream::KIStringStream(KIStringStream&& other)
    : base_type{std::move(other)}
    , m_sView{other.m_sView}
    , m_KIStreamBuf{std::move(other.m_KIStreamBuf)}
//-----------------------------------------------------------------------------
{
} // move ctor
#endif

//-----------------------------------------------------------------------------
KIStringStream::~KIStringStream()
//-----------------------------------------------------------------------------
{}

#if defined(DEKAF2_NO_GCC) || (DEKAF2_GCC_VERSION >= 50000)
//-----------------------------------------------------------------------------
KIStringStream& KIStringStream::operator=(KIStringStream&& other)
//-----------------------------------------------------------------------------
{
	m_sView = other.m_sView;
	m_KIStreamBuf = std::move(other.m_KIStreamBuf);
	return *this;
}
#endif

//-----------------------------------------------------------------------------
/// this is the custom KString reader
std::streamsize KIStringStream::KStringReader(void* sBuffer, std::streamsize iCount, void* sSourceBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (sSourceBuf != nullptr && sBuffer != nullptr)
	{
		char* pOutBuf = reinterpret_cast<char*>(sBuffer);
		KStringView* pInBuf = reinterpret_cast<KStringView*>(sSourceBuf);
		iWrote = std::min(static_cast<size_t>(iCount), pInBuf->size());
		pInBuf->copy(pOutBuf, iWrote, 0);
		pInBuf->remove_prefix(iWrote);
	}
	
	return iWrote;
}


} // end namespace dekaf2
