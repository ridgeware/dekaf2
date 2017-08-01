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

#include <fstream>
#include "kfdwriter.h"
#include "klog.h"

namespace dekaf2
{

//-----------------------------------------------------------------------------
KOutputFDStream::KOutputFDStream(KOutputFDStream&& other)
    : m_FileDesc{other.m_FileDesc}
    , m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FileDesc = -1;

} // move ctor

//-----------------------------------------------------------------------------
KOutputFDStream::~KOutputFDStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

//-----------------------------------------------------------------------------
KOutputFDStream& KOutputFDStream::operator=(KOutputFDStream&& other)
//-----------------------------------------------------------------------------
{
	m_FileDesc = other.m_FileDesc;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FileDesc = -1;
	return *this;
}

//-----------------------------------------------------------------------------
void KOutputFDStream::open(int iFileDesc)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it..

	m_FileDesc = iFileDesc;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KOutputFDStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FileDesc >= 0)
	{
		base_type::flush();
		if (::close(m_FileDesc))
		{
			KLog().warning("KOutputFDStream: Cannot close file: {}", strerror(errno));
		}
		m_FileDesc = -1;
	}

} // close

//-----------------------------------------------------------------------------
std::streamsize KOutputFDStream::FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (filedesc)
	{
		// it is more difficult than one would expect to convert a void* into an int..
		int fd = static_cast<int>(*static_cast<long*>(filedesc));
		iWrote = ::write(fd, sBuffer, static_cast<size_t>(iCount));
		if (iWrote != iCount)
		{
			// do some logging
			KLog().warning("KOutputFDStream: cannot write to file: {}", strerror(errno));
		}
	}

	return iWrote;
}


//-----------------------------------------------------------------------------
KOutputFPStream::KOutputFPStream(KOutputFPStream&& other)
    : m_FilePtr{other.m_FilePtr}
    , m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FilePtr = nullptr;

} // move ctor

//-----------------------------------------------------------------------------
KOutputFPStream::~KOutputFPStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

//-----------------------------------------------------------------------------
KOutputFPStream& KOutputFPStream::operator=(KOutputFPStream&& other)
//-----------------------------------------------------------------------------
{
	m_FilePtr = other.m_FilePtr;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FilePtr = nullptr;
	return *this;
}

//-----------------------------------------------------------------------------
void KOutputFPStream::open(FILE* iFilePtr)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it..

	m_FilePtr = iFilePtr;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KOutputFPStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FilePtr >= 0)
	{
		base_type::flush();
		if (::fclose(m_FilePtr))
		{
			KLog().warning("KOutputFPStream: Cannot close file: {}", strerror(errno));
		}
		m_FilePtr = nullptr;
	}

} // close

//-----------------------------------------------------------------------------
std::streamsize KOutputFPStream::FilePtrWriter(const void* sBuffer, std::streamsize iCount, void* fileptr)
//-----------------------------------------------------------------------------
{
	std::streamsize iWrote{0};

	if (fileptr)
	{
		FILE** fp = static_cast<FILE**>(fileptr);
		if (fp && *fp)
		{
			iWrote = static_cast<std::streamsize>(std::fwrite(sBuffer, 1, static_cast<size_t>(iCount), *fp));
			if (iWrote != iCount)
			{
				// do some logging
				KLog().warning("KOutputFPStream: cannot write to file: {}", strerror(errno));
			}
		}
	}

	return iWrote;
}


} // end of namespace dekaf2

