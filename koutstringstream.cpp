/*
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

#include "koutstringstream.h"
#include "kstring.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
std::streamsize detail::KStringWriter(const void* sBuffer, std::streamsize iCount, void* sTargetBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (sTargetBuf != nullptr && sBuffer != nullptr)
	{
		auto pInBuf = reinterpret_cast<const KString::value_type*>(sBuffer);
		auto pOutBuf = reinterpret_cast<KString**>(sTargetBuf);
		if (*pOutBuf != nullptr)
		{
			(*pOutBuf)->append(pInBuf, static_cast<size_t>(iCount));
			iWrote = iCount;
		}
		else
		{
			kWarning("no output string opened");
		}
	}

	return iWrote;
}

//-----------------------------------------------------------------------------
const KString& KOStringStream::str() const
//-----------------------------------------------------------------------------
{
	static const KString s_sEmpty{};

	return (m_sBuffer) ? *m_sBuffer : s_sEmpty;
}

//-----------------------------------------------------------------------------
KOStringStream::KOStringStream(KOStringStream&& other) noexcept
//-----------------------------------------------------------------------------
: base_type(&m_KOStreamBuf)
{
	m_sBuffer = other.m_sBuffer;
	other.m_sBuffer = nullptr;
}

//-----------------------------------------------------------------------------
void KOStringStream::str(KString sString)
//-----------------------------------------------------------------------------
{
	if (m_sBuffer)
	{
		*m_sBuffer = std::move(sString);
	}
	else
	{
		kWarning("no output string opened");
	}
}

} // end namespace dekaf2
