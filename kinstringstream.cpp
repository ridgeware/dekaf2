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

#include "kinstringstream.h"
#include "klog.h"

namespace dekaf2 {

//-----------------------------------------------------------------------------
/// this is the custom KString reader
std::streamsize detail::KStringReader(void* sBuffer, std::streamsize iCount, void* sSourceBuf)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead { 0 };

	if (sSourceBuf != nullptr && sBuffer != nullptr)
	{
		auto pOutBuf = reinterpret_cast<char*>(sBuffer);
		auto pInBuf = reinterpret_cast<KStringView*>(sSourceBuf);
		iRead = std::min(static_cast<size_t>(iCount), pInBuf->size());
		pInBuf->copy(pOutBuf, iRead, 0);
		pInBuf->remove_prefix(iRead);
	}
	
	return iRead;

} // detail::KStringReader


//-----------------------------------------------------------------------------
bool KInStringStreamBuf::open(KStringView sView)
//-----------------------------------------------------------------------------
{
	auto data = const_cast<char*>(sView.data());
	setg(data, data, data + sView.size());
	return true;

} // open

//-----------------------------------------------------------------------------
std::streambuf::int_type KInStringStreamBuf::underflow()
//-----------------------------------------------------------------------------
{
	// there is no further data..
	return traits_type::eof();

} // underflow

//-----------------------------------------------------------------------------
std::streamsize KInStringStreamBuf::xsgetn(char_type* s, std::streamsize n)
//-----------------------------------------------------------------------------
{
	// read as many chars as possible directly from the stream buffer
	// (keep in mind that the stream buffer is not necessarily our
	// own one-char buffer, but could be replaced by other stream buffers)
	std::streamsize iReadInStreamBuf = std::min(n, in_avail());

	if (iReadInStreamBuf > 0)
	{
		std::memcpy(s, gptr(), static_cast<size_t>(iReadInStreamBuf));
		s += iReadInStreamBuf;
		n -= iReadInStreamBuf;
		// adjust stream buffer pointers
		setg(eback(), gptr()+iReadInStreamBuf, egptr());
	}

	return iReadInStreamBuf;

} // xsgetn

//-----------------------------------------------------------------------------
std::streambuf::pos_type KInStringStreamBuf::seekoff(off_type off,
													 std::ios_base::seekdir dir,
													 std::ios_base::openmode which)
//-----------------------------------------------------------------------------
{
	if ((which & std::ios_base::in) == 0)
	{
		kDebug(1, "we only have an input arena");
		return pos_type(off_type(-1));
	}

	char_type* pRead;

	switch (dir)
	{
		case std::ios_base::beg:
			pRead = eback() + off;
			break;

		case std::ios_base::end:
			pRead = egptr() - off;
			break;

		case std::ios_base::cur:
			pRead = gptr() + off;
			break;

		default:
			// weird libstdc++ implementations on RH require
			// _S_ios_seekdir_end as switch case??
			kDebug(1, "bad stream library implementation");
			return pos_type(off_type(-1));
	}

	if (pRead < eback() || pRead > egptr())
	{
		kDebug(1, "offset out of range: {}", off);
		return pos_type(off_type(-1));
	}

	setg(eback(), pRead, egptr());

	return pRead - eback();

} // seekoff

} // end namespace dekaf2
