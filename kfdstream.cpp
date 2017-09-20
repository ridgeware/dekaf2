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


#include "kfdstream.h"
#include "klog.h"
#include <sys/stat.h>
#include <unistd.h>

namespace dekaf2
{


#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
// gcc 4.8.5 has troubles with moves..
//-----------------------------------------------------------------------------
KInputFDStream::KInputFDStream(KInputFDStream&& other)
	: m_FileDesc{other.m_FileDesc}
	, m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FileDesc = -1;

} // move ctor
#endif

//-----------------------------------------------------------------------------
KInputFDStream::~KInputFDStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KInputFDStream& KInputFDStream::operator=(KInputFDStream&& other)
//-----------------------------------------------------------------------------
{
	m_FileDesc = other.m_FileDesc;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FileDesc = -1;
	return *this;
}
#endif

//-----------------------------------------------------------------------------
void KInputFDStream::open(int iFileDesc)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it

	m_FileDesc = iFileDesc;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KInputFDStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FileDesc >= 0)
	{
		if (::close(m_FileDesc))
		{
			KLog().warning("KInputFDStream: Cannot close file: {}", strerror(errno));
		}
		m_FileDesc = -1;
	}

} // close

//-----------------------------------------------------------------------------
std::streamsize KInputFDStream::FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead{0};

	if (filedesc)
	{
		// it is more difficult than one would expect to convert a void* into an int..
		int fd = static_cast<int>(*static_cast<long*>(filedesc));
		iRead = ::read(fd, sBuffer, static_cast<size_t>(iCount));
		if (iRead < 0)
		{
			// do some logging
			KLog().warning("KInputFDStream: cannot read from file: {} - requested {}, got {} bytes",
			               strerror(errno),
			               iCount,
			               iRead);
		}
	}

	return iRead;
}


#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KInputFPStream::KInputFPStream(KInputFPStream&& other)
    : m_FilePtr{other.m_FilePtr}
    , m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FilePtr = nullptr;

} // move ctor
#endif

//-----------------------------------------------------------------------------
KInputFPStream::~KInputFPStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KInputFPStream& KInputFPStream::operator=(KInputFPStream&& other)
//-----------------------------------------------------------------------------
{
	m_FilePtr = other.m_FilePtr;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FilePtr = nullptr;
	return *this;
}
#endif

//-----------------------------------------------------------------------------
void KInputFPStream::open(FILE* iFilePtr)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it

	m_FilePtr = iFilePtr;

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KInputFPStream::close()
//-----------------------------------------------------------------------------
{
	if (m_FilePtr >= 0)
	{
		if (std::fclose(m_FilePtr))
		{
			KLog().warning("KInputFPStream: Cannot close file: {}", strerror(errno));
		}
		m_FilePtr = nullptr;
	}

} // close

//-----------------------------------------------------------------------------
std::streamsize KInputFPStream::FilePtrReader(void* sBuffer, std::streamsize iCount, void* fileptr)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead{0};

	if (fileptr)
	{
		FILE** fp = static_cast<FILE**>(fileptr);
		if (fp && *fp)
		{
			iRead = static_cast<std::streamsize>(std::fread(sBuffer, 1, static_cast<size_t>(iCount), *fp));
			if (iRead < 0)
			{
				// do some logging
				KLog().warning("KInputFPStream: cannot read from file: {} - requested {}, got {} bytes",
				               strerror(errno),
				               iCount,
				               iRead);
			}
		}
	}

	return iRead;
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KOutputFDStream::KOutputFDStream(KOutputFDStream&& other)
    : m_FileDesc{other.m_FileDesc}
    , m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FileDesc = -1;

} // move ctor
#endif

//-----------------------------------------------------------------------------
KOutputFDStream::~KOutputFDStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KOutputFDStream& KOutputFDStream::operator=(KOutputFDStream&& other)
//-----------------------------------------------------------------------------
{
	m_FileDesc = other.m_FileDesc;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FileDesc = -1;
	return *this;
}
#endif

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


#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KOutputFPStream::KOutputFPStream(KOutputFPStream&& other)
    : m_FilePtr{other.m_FilePtr}
    , m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FilePtr = nullptr;

} // move ctor
#endif

//-----------------------------------------------------------------------------
KOutputFPStream::~KOutputFPStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KOutputFPStream& KOutputFPStream::operator=(KOutputFPStream&& other)
//-----------------------------------------------------------------------------
{
	m_FilePtr = other.m_FilePtr;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FilePtr = nullptr;
	return *this;
}
#endif

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

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
// gcc 4.8.5 has troubles with moves..
//-----------------------------------------------------------------------------
KInOutFDStream::KInOutFDStream(KInOutFDStream&& other)
    : m_FileDescR{other.m_FileDescR}
    , m_FileDescW{other.m_FileDescW}
	, m_FPStreamBuf{std::move(other.m_FPStreamBuf)}
//-----------------------------------------------------------------------------
{
	other.m_FileDescR = -1;
	other.m_FileDescW = -1;

} // move ctor
#endif

//-----------------------------------------------------------------------------
KInOutFDStream::~KInOutFDStream()
//-----------------------------------------------------------------------------
{
	// do not call close on destruction. This class did not open the file
	// but just received a handle for it
}

#if !defined(__GNUC__) || (DEKAF2_GCC_VERSION >= 500)
//-----------------------------------------------------------------------------
KInOutFDStream& KInOutFDStream::operator=(KInOutFDStream&& other)
//-----------------------------------------------------------------------------
{
	m_FileDescR = other.m_FileDescR;
	m_FileDescW = other.m_FileDescW;
	m_FPStreamBuf = std::move(other.m_FPStreamBuf);
	other.m_FileDescR = -1;
	other.m_FileDescW = -1;
	return *this;
}
#endif

//-----------------------------------------------------------------------------
void KInOutFDStream::open(int iFileDescR, int iFileDescW)
//-----------------------------------------------------------------------------
{
	// do not close the stream here - this class did not open it

	m_FileDescR = iFileDescR;

	if (iFileDescW < 0)
	{
		m_FileDescW = m_FileDescR;
	}
	else
	{
		m_FileDescW = iFileDescW;
	}

	base_type::init(&m_FPStreamBuf);

} // open

//-----------------------------------------------------------------------------
void KInOutFDStream::close()
//-----------------------------------------------------------------------------
{
	bool bSameFD = m_FileDescR == m_FileDescW;

	if (m_FileDescR >= 0)
	{
		if (::close(m_FileDescR))
		{
			KLog().warning("KInOutFDStream: Cannot close file: {}", strerror(errno));
		}
		m_FileDescR = -1;
	}

	if (!bSameFD)
	{
		if (m_FileDescW >= 0)
		{
			if (::close(m_FileDescW))
			{
				KLog().warning("KInOutFDStream: Cannot close file: {}", strerror(errno));
			}
		}
	}
	m_FileDescW = -1;

} // close

//-----------------------------------------------------------------------------
std::streamsize KInOutFDStream::FileDescReader(void* sBuffer, std::streamsize iCount, void* filedesc)
//-----------------------------------------------------------------------------
{
	std::streamsize iRead{0};

	if (filedesc)
	{
		// it is more difficult than one would expect to convert a void* into an int..
		int fd = static_cast<int>(*static_cast<long*>(filedesc));
		iRead = ::read(fd, sBuffer, static_cast<size_t>(iCount));
		if (iRead < 0)
		{
			// do some logging
			KLog().warning("KInOutFDStream: cannot read from file: {} - requested {}, got {} bytes",
			               strerror(errno),
			               iCount,
			               iRead);
		}
	}

	return iRead;
}

//-----------------------------------------------------------------------------
std::streamsize KInOutFDStream::FileDescWriter(const void* sBuffer, std::streamsize iCount, void* filedesc)
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
			KLog().warning("InOutFDStream: cannot write to file: {}", strerror(errno));
		}
	}

	return iWrote;
}



} // end of namespace dekaf2

