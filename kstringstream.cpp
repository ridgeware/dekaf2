/*
//
// DEKAF(tm): Lighter, Faster, Smarter (tm)
//
// Copyright (c) 2019, Ridgeware, Inc.
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
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
std::streamsize detail::KIOStringReader(void* sBuffer, std::streamsize iCount, void* sSourceBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead { 0 };

	if (sSourceBuf != nullptr && sBuffer != nullptr)
	{
		auto pOutBuf = reinterpret_cast<char*>(sBuffer);
		auto pBuf = reinterpret_cast<KIOStringStream::Buffer*>(sSourceBuf);
		if (*pBuf->sBuffer && pBuf->iReadPos < pBuf->sBuffer->size())
		{
			KStringView sBuffer(*pBuf->sBuffer);
			sBuffer.remove_prefix(pBuf->iReadPos);
			iRead = std::min(static_cast<size_t>(iCount), sBuffer.size());
			sBuffer.copy(pOutBuf, iRead, 0);
			pBuf->iReadPos += iRead;
		}
	}

	return iRead;

} // detail::KStringReader

//-----------------------------------------------------------------------------
KIOStringStream::KIOStringStream(KIOStringStream&& other) noexcept
//-----------------------------------------------------------------------------
: base_type(&m_KStreamBuf)
{
	m_Buffer = other.m_Buffer;
}

//-----------------------------------------------------------------------------
const KString& KIOStringStream::str() const
//-----------------------------------------------------------------------------
{
	static const KString s_sEmpty{};

	return (m_Buffer.sBuffer) ? *m_Buffer.sBuffer : s_sEmpty;
}

//-----------------------------------------------------------------------------
void KIOStringStream::str(KStringView sView)
//-----------------------------------------------------------------------------
{
	if (m_Buffer.sBuffer)
	{
		*m_Buffer.sBuffer = sView;
	}
	else
	{
		kWarning("no output string opened");
	}
}

} // end namespace dekaf2
