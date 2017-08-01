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


#include "kreader.h"
#include "klog.h"
#include <sys/stat.h>

namespace dekaf2
{

//-----------------------------------------------------------------------------
bool kRewind(std::istream& Stream)
//-----------------------------------------------------------------------------
{
	return Stream.rdbuf() && Stream.rdbuf()->pubseekoff(0, std::ios_base::beg) != std::streambuf::pos_type(std::streambuf::off_type(-1));
}

//-----------------------------------------------------------------------------
ssize_t kGetSize(std::istream& Stream, bool bFromStart)
//-----------------------------------------------------------------------------
{
	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		return std::streambuf::pos_type(std::streambuf::off_type(-1));
	}

	std::streambuf::pos_type curPos = sb->pubseekoff(0, std::ios_base::cur);
	if (curPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return curPos;
	}

	std::streambuf::pos_type endPos = sb->pubseekoff(0, std::ios_base::end);
	if (endPos == std::streambuf::pos_type(std::streambuf::off_type(-1)))
	{
		return endPos;
	}

	if (endPos != curPos)
	{
		sb->pubseekoff(curPos, std::ios_base::beg);
	}

	if (bFromStart)
	{
		return endPos;
	}
	else
	{
		return endPos - curPos;
	}
}

//-----------------------------------------------------------------------------
bool kReadAll(std::istream& Stream, KString& sContent, bool bFromStart)
//-----------------------------------------------------------------------------
{
	sContent.clear();

	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		return false;
	}

	// get size of the file.
	ssize_t iSize = kGetSize(Stream, bFromStart);

	if (iSize < 0)
	{
		return false;
	}

	if (!iSize)
	{
		return true;
	}

	// position stream to the beginning
	if (bFromStart && !kRewind(Stream))
	{
		return false;
	}

	size_t uiSize = static_cast<size_t>(iSize);

	// create the read buffer
	sContent.resize(uiSize);

	size_t iRead = static_cast<size_t>(sb->sgetn(sContent.data(), iSize));

	// we should now be at the end of the input..
	if (std::istream::traits_type::eq_int_type(sb->sgetc(), std::istream::traits_type::eof()))
	{
		Stream.setstate(std::ios_base::eofbit);
	}
	else
	{
		KLog().warning ("kReadAll: stream grew during read, did not read all new content");
	}

	if (iRead != uiSize)
	{
		KLog().warning ("KReader: Unable to read full file, requested {0} bytes, got {1}", iSize, iRead);
		return false;
	}
/*
	if (bIsText) // dos2unix
	{
		sContent.Replace("\r\n", "\n", true);
	}
*/
	return true;

} // ReadAll

//-----------------------------------------------------------------------------
bool kReadLine(std::istream& Stream, KString& sLine, KStringView sTrimRight, KString::value_type delimiter)
//-----------------------------------------------------------------------------
{
	sLine.clear();

	std::streambuf* sb = Stream.rdbuf();

	if (!sb)
	{
		return false;
	}

	for (;;)
	{
		std::streambuf::int_type iCh = sb->sbumpc();
		if (std::streambuf::traits_type::eq_int_type(iCh, std::streambuf::traits_type::eof()))
		{
			// the EOF case
			Stream.setstate(std::ios_base::eofbit);

			bool bSuccess = !sLine.empty();

			if (!sTrimRight.empty())
			{
				sLine.TrimRight(sTrimRight);
			}

			return bSuccess;
		}

		KString::value_type Ch = static_cast<KString::value_type>(iCh);

		sLine += Ch;

		if (Ch == delimiter)
		{
			if (!sTrimRight.empty())
			{
				sLine.TrimRight(sTrimRight);
			}
			return true;
		}
	}
}

} // end of namespace dekaf2

