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

#include "kdataprovider.h"
#include "kread.h"
#include "kwrite.h"

DEKAF2_NAMESPACE_BEGIN

//-----------------------------------------------------------------------------
KDataProvider::~KDataProvider()
//-----------------------------------------------------------------------------
{
} // dtor

//-----------------------------------------------------------------------------
std::size_t KViewProvider::Read(void* buffer, std::size_t size)
//-----------------------------------------------------------------------------
{
	auto iRead = CalcNextReadSize(size);
	std::memcpy(buffer, m_sView.data(), iRead);
	m_sView.remove_prefix(iRead);
	return iRead;

} // Read

//-----------------------------------------------------------------------------
std::size_t KViewProvider::Read(std::ostream& ostream, std::size_t size)
//-----------------------------------------------------------------------------
{
	auto iRead  = CalcNextReadSize(size);
	auto iWrote = kWrite(ostream, m_sView.data(), iRead);
	m_sView.remove_prefix(iRead);
	return iWrote;

} // Read

//-----------------------------------------------------------------------------
bool KViewProvider::Read(KStringView& sView, std::size_t size)
//-----------------------------------------------------------------------------
{
	auto iRead  = CalcNextReadSize(size);
	sView = KStringView(m_sView.data(), iRead);
	m_sView.remove_prefix(iRead);
	return true;

} // Read

//-----------------------------------------------------------------------------
std::size_t KIStreamProvider::Read(void* buffer, std::size_t size)
//-----------------------------------------------------------------------------
{
	size = std::min(size, m_iMaxRead);
	size = kRead(m_IStream, buffer, size);
	m_iMaxRead -= size;
	return size;

} // Read

//-----------------------------------------------------------------------------
bool KIStreamProvider::IsEOF () const
//-----------------------------------------------------------------------------
{
	return !m_iMaxRead || m_IStream.eof();

} // IsEOF

DEKAF2_NAMESPACE_END
